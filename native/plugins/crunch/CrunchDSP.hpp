/*
 * NULL JSFX - Crunch, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/crunch.jsfx, computed in double like EEL2:
 *
 *   clip(x, k) = x / (1 + |x|^k)^(1/k)
 *   drive = 1 + 19a,  knee = 2 + 8a,  comp = 0.7/clip(0.7*drive, knee)
 *   w     = lp9k(clip(x*drive, knee)) * comp     (one-pole, c = 1 - exp(-2 pi 9000/sr))
 *   y     = x + a*(w - x),  a = Amount/100
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullcrunch {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

inline double clip(double x, double k) noexcept
{
    return x / std::pow(1.0 + std::pow(std::fabs(x), k), 1.0 / k);
}

struct Params {
    double amount, drive, knee, comp;
};

// the JSFX @slider section (without the filter coefficient)
inline Params paramsFor(double slider) noexcept
{
    Params p;
    p.amount = slider / 100.0;
    p.drive = 1.0 + p.amount * 19.0;
    p.knee = 2.0 + p.amount * 8.0;
    p.comp = 0.7 / clip(p.drive * 0.7, p.knee);
    return p;
}

// static transfer curve (the 9 kHz roll-off left out), for the display
inline double curve(const Params& p, double x) noexcept
{
    return x + p.amount * (clip(x * p.drive, p.knee) * p.comp - x);
}

class CrunchProcessor {
public:
    void setSampleRate(double sr) noexcept { fC = 1.0 - std::exp(-2.0 * kPi * 9000.0 / sr); }
    void setAmount(double slider) noexcept { fP = paramsFor(slider); }
    void reset() noexcept { fYL = fYR = 0.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Params p = fP;
        const double c = fC;
        for (uint32_t i = 0; i < frames; ++i) {
            double l = inL[i], r = inR[i];
            fYL += c * (clip(l * p.drive, p.knee) - fYL);
            const double wl = fYL * p.comp;
            fYR += c * (clip(r * p.drive, p.knee) - fYR);
            const double wr = fYR * p.comp;
            l += p.amount * (wl - l);
            r += p.amount * (wr - r);
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    Params fP = paramsFor(50.0);
    double fC = 1.0 - std::exp(-2.0 * kPi * 9000.0 / 48000.0);
    double fYL = 0.0, fYR = 0.0;
};

} // namespace nullcrunch
