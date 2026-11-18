/*
 * NULL JSFX - Stereo Width, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/stereo_width.jsfx, computed in double like EEL2: mid/side
 * width (0 to 200 %) with a dry/wet mix. With a crossover above 0 Hz the side signal below it
 * is dropped (a one-pole split) so the bass stays mono and only the highs are widened.
 * High Crossover above 0 Hz splits the widened side once more (one pole): above it the side
 * gets High Width instead of Width (off at 0 Hz, the default).
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullstereowidth {

struct Params {
    double width = 1.0, mix = 1.0, xover = 0.0, coef = 0.0;
    double hxover = 0.0, hwidth = 1.0, hcoef = 0.0;
};

// the JSFX's @slider
inline Params paramsFor(double widthPct, double mixPct, double xoverHz, double sr,
                        double highXoverHz = 0.0, double highWidthPct = 100.0) noexcept
{
    Params p;
    p.width = widthPct / 100.0;
    p.mix = mixPct / 100.0;
    p.xover = xoverHz;
    p.coef = p.xover > 0.0 ? 1.0 - std::exp(-2.0 * 3.14159265358979 * p.xover / sr) : 0.0;
    p.hxover = highXoverHz;
    p.hwidth = highWidthPct / 100.0;
    p.hcoef = p.hxover > 0.0 ? 1.0 - std::exp(-2.0 * 3.14159265358979 * std::fmin(p.hxover, 0.45 * sr) / sr) : 0.0;
    return p;
}

// one-pole high-pass of coefficient c at frequency f (1 when c = 0)
inline void onePoleHp(double c, double f, double sr, double& hr, double& hi) noexcept
{
    hr = 1.0;
    hi = 0.0;
    if (c > 0.0)
    {
        const double w = 2.0 * M_PI * std::fmin(f, sr * 0.499) / sr;
        const double b = 1.0 - c;
        const double dr = 1.0 - b * std::cos(w), di = b * std::sin(w);
        const double dd = dr * dr + di * di;
        hr = 1.0 - c * dr / dd;
        hi = c * di / dd;
    }
}

// side gain (1 = unchanged) at frequency f: |(1 - mix) + mix * HP1 * (width + (high width -
// width) * HP2)| with HP1 and HP2 the one-pole high-passes of the two crossovers; the mid is
// untouched (as sw_side in the GFX)
inline double sideGain(const Params& p, double f, double sr) noexcept
{
    double hr, hi;
    onePoleHp(p.coef, f, sr, hr, hi);
    double gr = p.width, gi = 0.0;
    if (p.hxover > 0.0)
    {
        double h2r, h2i;
        onePoleHp(p.hcoef, f, sr, h2r, h2i);
        gr += (p.hwidth - p.width) * h2r;
        gi = (p.hwidth - p.width) * h2i;
    }
    const double tr = (1.0 - p.mix) + p.mix * (hr * gr - hi * gi);
    const double ti = p.mix * (hr * gi + hi * gr);
    return std::sqrt(tr * tr + ti * ti);
}

class StereoWidthProcessor {
public:
    void reset() { fLpMid = fLpSide = fLpHs = 0.0; } // @init

    void setParams(const Params& p) { fP = p; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        const double w = fP.width, m = fP.mix, c = fP.coef;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            const double mid = (l + r) * 0.5, side = (l - r) * 0.5;
            double ol, orr, hbIn = side;
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
                hbIn = hpSide;
            }
            else
            {
                ol = mid + side * w;
                orr = mid - side * w;
            }
            if (fP.hxover > 0.0)
            {
                // the high band: the side above High Crossover gets High Width instead of Width
                fLpHs = fLpHs + fP.hcoef * (hbIn - fLpHs);
                const double hb = (hbIn - fLpHs) * (fP.hwidth - w);
                ol += hb;
                orr -= hb;
            }
            outL[i] = static_cast<T>(l * (1.0 - m) + ol * m);
            outR[i] = static_cast<T>(r * (1.0 - m) + orr * m);
        }
    }

private:
    Params fP;
    double fLpMid = 0.0, fLpSide = 0.0, fLpHs = 0.0;
};

} // namespace nullstereowidth
