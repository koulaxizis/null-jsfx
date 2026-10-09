/*
 * NULL JSFX - Driver, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/driver.jsfx, computed in double like EEL2:
 *
 *   sat(x) = (e - 1)/(e + 1),  e = exp(2*clamp(x, -20, 20))       (tanh)
 *   drive  = 1 + 11a,  bias = 0.25a,  comp = 0.5/(sat(drive/2 + bias) - sat(bias))
 *   w      = dcblock((sat(x*drive + bias) - sat(bias)) * comp)
 *   y      = x + a*(w - x),  a = Amount/100
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulldriver {

inline double sat(double x) noexcept
{
    const double e = std::exp(2.0 * std::max(-20.0, std::min(20.0, x)));
    return (e - 1.0) / (e + 1.0);
}

struct Params {
    double amount, drive, bias, comp;
};

// the JSFX @slider section
inline Params paramsFor(double slider) noexcept
{
    Params p;
    p.amount = slider / 100.0;
    p.drive = 1.0 + p.amount * 11.0;
    p.bias = 0.25 * p.amount;
    p.comp = 1.0 / (sat(p.drive * 0.5 + p.bias) - sat(p.bias)) * 0.5;
    return p;
}

// static transfer curve (the DC blocker left out), for the display
inline double curve(const Params& p, double x) noexcept
{
    return x + p.amount * ((sat(x * p.drive + p.bias) - sat(p.bias)) * p.comp - x);
}

class DriverProcessor {
public:
    void setAmount(double slider) noexcept { fP = paramsFor(slider); }

    void reset() noexcept { fX1L = fY1L = fX1R = fY1R = 0.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Params p = fP;
        const double sb = sat(p.bias);
        for (uint32_t i = 0; i < frames; ++i) {
            double l = inL[i], r = inR[i];
            const double wl = dcb((sat(l * p.drive + p.bias) - sb) * p.comp, fX1L, fY1L);
            const double wr = dcb((sat(r * p.drive + p.bias) - sb) * p.comp, fX1R, fY1R);
            l += p.amount * (wl - l);
            r += p.amount * (wr - r);
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    static double dcb(double x, double& x1, double& y1) noexcept
    {
        const double y = x - x1 + 0.9995 * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    Params fP = paramsFor(50.0);
    double fX1L = 0.0, fY1L = 0.0, fX1R = 0.0, fY1R = 0.0;
};

} // namespace nulldriver
