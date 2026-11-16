/*
 * NULL JSFX - Warmer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/warmer.jsfx (computed in double, like EEL2): the lows and low
 * mids (below 600 Hz) through a gentle, slightly asymmetric tanh curve (its DC removed) and a
 * little louder, the top eased down and a soft low-pass from 20 to 9 kHz.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullwarmer {

// tanh as the JSFX computes it: (e^2x - 1) / (e^2x + 1), x clamped to +-20
inline double sat(double x) noexcept
{
    const double e = std::exp(2.0 * std::max(-20.0, std::min(20.0, x)));
    return (e - 1.0) / (e + 1.0);
}

struct Params {
    double a, d, b, z, norm, lo, hi, tc, tm, trim;
};

// @slider
inline Params paramsFor(double slider, double sr) noexcept
{
    Params p;
    p.a = slider / 100.0;
    p.d = 1.0 + p.a * 2.0;
    p.b = 0.15 * p.a;
    p.z = sat(p.b);
    p.norm = 1.0 / (p.d * (1.0 - p.z * p.z));
    p.lo = 1.0 + 0.3 * p.a;
    p.hi = 1.0 - 0.2 * p.a;
    p.tc = 1.0 - std::exp(-2.0 * M_PI * std::min(20000.0 - 11000.0 * p.a, 0.45 * sr) / sr);
    p.tm = std::min(1.0, p.a * 2.0);
    p.trim = 1.0 - 0.08 * p.a;
    return p;
}

// the low band's curve for an input x (DC blocker left out), for the display
inline double curve(const Params& p, double x) noexcept
{
    return x + p.a * ((sat(p.d * x + p.b) - p.z) * p.norm - x);
}

struct Channel {
    double lo = 0, x1 = 0, y1 = 0, t = 0;
};

class WarmerProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fLpc = 1.0 - std::exp(-2.0 * M_PI * std::min(600.0, 0.45 * sr) / sr);
        fDcr = std::exp(-2.0 * M_PI * 10.0 / sr);
        fC[0] = fC[1] = Channel();
        fP = paramsFor(fAmount, sr);
    }

    void setAmount(double v) noexcept
    {
        fAmount = v;
        fP = paramsFor(v, fSr);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            outL[i] = static_cast<T>(tick(fC[0], double(inL[i])));
            outR[i] = static_cast<T>(tick(fC[1], double(inR[i])));
        }
    }

private:
    double tick(Channel& c, double x) noexcept
    {
        const Params& p = fP;
        c.lo += (x - c.lo) * fLpc;
        const double l = c.lo;
        const double s0 = (sat(p.d * l + p.b) - p.z) * p.norm;
        const double dc = s0 - c.x1 + fDcr * c.y1;
        c.x1 = s0;
        c.y1 = dc;
        const double s = l + p.a * (dc - l);
        const double y = s * p.lo + (x - l) * p.hi;
        c.t += (y - c.t) * p.tc;
        return (y + (c.t - y) * p.tm) * p.trim;
    }

    double fSr = 44100.0, fAmount = 40.0, fLpc = 0.0, fDcr = 0.0;
    Params fP = paramsFor(40.0, 44100.0);
    Channel fC[2];
};

} // namespace nullwarmer
