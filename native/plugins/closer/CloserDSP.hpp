/*
 * NULL JSFX - Closer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/closer.jsfx (computed in double, like EEL2): a light compressor,
 * a presence band around 3.5 kHz that eases off while the compressor works hard, and a
 * proximity lift below 180 Hz.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullcloser {

struct Coeffs {
    double b0, b2, a1, a2, lc;
};

// @init: the band-pass (0 dB at its peak, Q 0.9) and the proximity low-pass
inline Coeffs coeffsFor(double sr) noexcept
{
    const double f = std::min(3500.0, 0.45 * sr);
    const double w = 2.0 * M_PI * f / sr;
    const double al = std::sin(w) / (2.0 * 0.9);
    const double a0 = 1.0 + al;
    Coeffs c;
    c.b0 = al / a0;
    c.b2 = -al / a0;
    c.a1 = -2.0 * std::cos(w) / a0;
    c.a2 = (1.0 - al) / a0;
    c.lc = 1.0 - std::exp(-2.0 * M_PI * std::min(180.0, 0.45 * sr) / sr);
    return c;
}

struct Params {
    double a, ratio, kp, kl, mu;
};

// @slider
inline Params paramsFor(double slider) noexcept
{
    Params p;
    p.a = slider / 100.0;
    p.ratio = 1.0 + 2.0 * p.a;
    p.kp = std::pow(10.0, p.a * 5.0 / 20.0) - 1.0;
    p.kl = std::pow(10.0, p.a * 2.5 / 20.0) - 1.0;
    p.mu = std::pow(10.0, p.a * 3.0 / 20.0);
    return p;
}

// the display: |1 + k * band-pass + proximity * low-pass| in dB at f
inline double responseDb(const Coeffs& c, const Params& p, double k, double f, double sr) noexcept
{
    const double w = 2.0 * M_PI * std::min(f, sr * 0.499) / sr;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    double nr = c.b0 + c.b2 * c2, ni = -(c.b2 * s2);
    double dr = 1.0 + c.a1 * c1 + c.a2 * c2, di = -(c.a1 * s1 + c.a2 * s2);
    double d = std::max(dr * dr + di * di, 1e-10);
    const double br = (nr * dr + ni * di) / d, bi = (ni * dr - nr * di) / d;
    dr = 1.0 - (1.0 - c.lc) * c1;
    di = (1.0 - c.lc) * s1;
    d = std::max(dr * dr + di * di, 1e-10);
    const double lr = c.lc * dr / d, li = -c.lc * di / d;
    nr = 1.0 + k * br + p.kl * lr;
    ni = k * bi + p.kl * li;
    return 10.0 * std::log10(std::max(nr * nr + ni * ni, 1e-10));
}

struct Band {
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double tick(const Coeffs& c, double x) noexcept
    {
        const double y = c.b0 * x + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

class CloserProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fC = coeffsFor(sr);
        fAtt = 1.0 - std::exp(-1.0 / (0.005 * sr));
        fRel = 1.0 - std::exp(-1.0 / (0.12 * sr));
        fE = 0.0;
        fBl = fBr = Band();
        fLl = fLr = 0.0;
    }

    void setAmount(double v) noexcept { fP = paramsFor(v); }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Params p = fP;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double s0 = inL[i], s1 = inR[i];
            const double in = std::max(std::fabs(s0), std::fabs(s1));
            fE += (in - fE) * (in > fE ? fAtt : fRel);
            const double over = 20.0 * std::log10(std::max(fE, 0.000001)) - kThr;
            const double gr = over > 0.0 ? over * (1.0 - 1.0 / p.ratio) : 0.0;
            const double g = std::pow(10.0, -gr / 20.0) * p.mu;
            const double pr = p.kp * (1.0 - 0.5 * std::min(1.0, gr / 6.0));
            const double l = s0 * g, r = s1 * g;
            const double bl = fBl.tick(fC, l);
            fLl += (l - fLl) * fC.lc;
            const double br = fBr.tick(fC, r);
            fLr += (r - fLr) * fC.lc;
            outL[i] = static_cast<T>(l + bl * pr + fLl * p.kl);
            outR[i] = static_cast<T>(r + br * pr + fLr * p.kl);
        }
    }

private:
    static constexpr double kThr = -24.0;
    Coeffs fC = coeffsFor(44100.0);
    Params fP = paramsFor(40.0);
    double fAtt = 0.0, fRel = 0.0, fE = 0.0, fLl = 0.0, fLr = 0.0;
    Band fBl, fBr;
};

} // namespace nullcloser
