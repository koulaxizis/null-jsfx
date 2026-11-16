/*
 * NULL JSFX - Smoother, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/smoother.jsfx (computed in double, like EEL2): four bands (2,
 * 3.15, 5 and 8 kHz, Q 2) watch their share of the mono sum and are cut, one after the other,
 * by what that share rises over a threshold.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullsmoother {

static constexpr double kFreq[4] = { 2000.0, 3150.0, 5000.0, 8000.0 };

struct Coeffs {
    double b0, a1, a2;
};

// @init: band k, a band-pass biquad with 0 dB at its peak (b2 = -b0)
inline Coeffs coeffsFor(int k, double sr) noexcept
{
    const double f = std::min(kFreq[k], 0.45 * sr);
    const double w = 2.0 * M_PI * f / sr;
    const double al = std::sin(w) / (2.0 * 2.0);
    return { al / (1.0 + al), -2.0 * std::cos(w) / (1.0 + al), (1.0 - al) / (1.0 + al) };
}

struct Params {
    double a, thr, max;
};

// @slider
inline Params paramsFor(double slider) noexcept
{
    const double a = slider / 100.0;
    return { a, -14.0 - 6.0 * a, 10.0 * a };
}

// the display: the deepest the four cuts can go together, in dB at f
inline double deepestDb(const Params& p, double f, double sr) noexcept
{
    const double w = 2.0 * M_PI * std::min(f, sr * 0.499) / sr;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    const double g = 1.0 - std::pow(10.0, -p.max / 20.0);
    double pr = 1.0, pi = 0.0;
    for (int k = 0; k < 4; ++k)
    {
        const Coeffs c = coeffsFor(k, sr);
        const double nr = c.b0 * (1.0 - c2), ni = c.b0 * s2;
        const double dr = 1.0 + c.a1 * c1 + c.a2 * c2, di = -(c.a1 * s1 + c.a2 * s2);
        const double d = std::max(dr * dr + di * di, 1e-10);
        const double hr = 1.0 - g * (nr * dr + ni * di) / d, hi = -g * (ni * dr - nr * di) / d;
        const double tr = pr * hr - pi * hi, ti = pr * hi + pi * hr;
        pr = tr;
        pi = ti;
    }
    return 10.0 * std::log10(std::max(pr * pr + pi * pi, 1e-10));
}

struct Band {
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double tick(const Coeffs& c, double x) noexcept
    {
        const double y = c.b0 * (x - x2) - c.a1 * y1 - c.a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

class SmootherProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fAtt = 1.0 - std::exp(-1.0 / (0.002 * sr));
        fRel = 1.0 - std::exp(-1.0 / (0.08 * sr));
        for (int k = 0; k < 4; ++k)
        {
            fC[k] = coeffsFor(k, sr);
            fDet[k] = fL[k] = fR[k] = Band();
            fEb[k] = 0.0;
        }
        fE = 0.0;
    }

    void setAmount(double v) noexcept { fP = paramsFor(v); }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Params p = fP;
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            const double m = (l + r) * 0.5;
            const double pw = m * m;
            fE += (pw - fE) * (pw > fE ? fAtt : fRel);
            for (int k = 0; k < 4; ++k)
            {
                double b = fDet[k].tick(fC[k], m);
                b *= b;
                fEb[k] += (b - fEb[k]) * (b > fEb[k] ? fAtt : fRel);
                const double rel = 10.0 * std::log10(std::max(fEb[k], 0.0000000001) / std::max(fE, 0.0000000001));
                const double red = std::min(p.max, std::max(0.0, (rel - p.thr) * p.a));
                const double g = 1.0 - std::pow(10.0, -red / 20.0);
                l -= fL[k].tick(fC[k], l) * g;
                r -= fR[k].tick(fC[k], r) * g;
            }
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    Coeffs fC[4] = {};
    Band fDet[4], fL[4], fR[4];
    double fEb[4] = {};
    Params fP = paramsFor(40.0);
    double fAtt = 0.0, fRel = 0.0, fE = 0.0;
};

} // namespace nullsmoother
