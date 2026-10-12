/*
 * NULL JSFX - Stereo Width, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/stereo_width.jsfx, computed in double like EEL2: mid/side
 * width (0 to 200 %) with a dry/wet mix. With a crossover above 0 Hz the side signal below it
 * is dropped (a one-pole split) so the bass stays mono and only the highs are widened.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullstereowidth {

struct Params {
    double width = 1.0, mix = 1.0, xover = 0.0, coef = 0.0;
};

// the JSFX's @slider
inline Params paramsFor(double widthPct, double mixPct, double xoverHz, double sr) noexcept
{
    Params p;
    p.width = widthPct / 100.0;
    p.mix = mixPct / 100.0;
    p.xover = xoverHz;
    p.coef = p.xover > 0.0 ? 1.0 - std::exp(-2.0 * 3.14159265358979 * p.xover / sr) : 0.0;
    return p;
}

// side gain (1 = unchanged) at frequency f: |(1 - mix) + mix * width * HP(f)| with HP the
// one-pole high-pass of the crossover (1 without one); the mid is untouched
inline double sideGain(const Params& p, double f, double sr) noexcept
{
    double hr = 1.0, hi = 0.0;
    if (p.xover > 0.0)
    {
        const double w = 2.0 * M_PI * std::fmin(f, sr * 0.499) / sr;
        const double b = 1.0 - p.coef;
        const double dr = 1.0 - b * std::cos(w), di = b * std::sin(w);
        const double dd = dr * dr + di * di;
        hr = 1.0 - p.coef * dr / dd;
        hi = p.coef * di / dd;
    }
    const double tr = (1.0 - p.mix) + p.mix * p.width * hr;
    hi *= p.mix * p.width;
    return std::sqrt(tr * tr + hi * hi);
}

class StereoWidthProcessor {
public:
    void reset() { fLpMid = fLpSide = 0.0; } // @init

    void setParams(const Params& p) { fP = p; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        const double w = fP.width, m = fP.mix, c = fP.coef;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            const double mid = (l + r) * 0.5, side = (l - r) * 0.5;
            double ol, orr;
            if (fP.xover > 0.0)
            {
                fLpMid = fLpMid + c * (mid - fLpMid);
                const double hpMid = mid - fLpMid;
                fLpSide = fLpSide + c * (side - fLpSide);
                const double hpSide = side - fLpSide;
                ol = fLpMid;
                orr = fLpMid;
                ol += hpMid + hpSide * w;
                orr += hpMid - hpSide * w;
            }
            else
            {
                ol = mid + side * w;
                orr = mid - side * w;
            }
            outL[i] = static_cast<T>(l * (1.0 - m) + ol * m);
            outR[i] = static_cast<T>(r * (1.0 - m) + orr * m);
        }
    }

private:
    Params fP;
    double fLpMid = 0.0, fLpSide = 0.0;
};

} // namespace nullstereowidth
