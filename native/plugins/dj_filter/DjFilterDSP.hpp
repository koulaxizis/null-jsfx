/*
 * NULL JSFX - DJ Filter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/dj_filter.jsfx, computed in double like EEL2:
 *
 *   pos  glides (20 ms one-pole, snapped within 1e-4) to Filter / 100
 *   low-pass  fc = 20000 * (60/20000)^d,  d = max(0, -pos)    two TPT SVF stages,
 *   high-pass fc = 20 * (10000/20)^d,     d = max(0,  pos)    Q 0.5412 and 1.3066 + 2.5 d
 *   mix  = min(1, |pos| * 20);  y = x + ((pos < 0 ? lp : hp) - x) * mix,  y = x when mix = 0
 *
 * Both chains always run, so neither starts from a stale state when the knob crosses 0.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulldjfilter {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

// cutoff (Hz) at position p (-1..1), as dj_fc in the GFX JSFX
inline double cutoff(double p, double sr) noexcept
{
    return p < 0.0 ? std::min(20000.0 * std::pow(60.0 / 20000.0, 0.0 - p), sr * 0.45)
                   : std::min(20.0 * std::pow(10000.0 / 20.0, p), sr * 0.45);
}

// display: magnitude (dB) at f of the filter at position p, dry mix included (dj_db in the
// GFX JSFX): the bilinear transform of 1 / (s^2 + k s + 1) or s^2 / (s^2 + k s + 1), two stages
inline double responseDb(double f, double p, double sr) noexcept
{
    const double d = std::fabs(p);
    const double o = std::tan(kPi * std::min(f, sr * 0.4999) / sr) / std::tan(kPi * cutoff(p, sr) / sr);
    const double k1 = 1.0 / 0.5412;
    const double k2 = 1.0 / (1.3066 + 2.5 * d);
    const double dr = 1.0 - o * o;
    const double pr = dr * dr - k1 * k2 * o * o;
    const double pq = o * dr * (k1 + k2);
    const double nr = p < 0.0 ? 1.0 : o * o * o * o;
    const double den = std::max(pr * pr + pq * pq, 1e-20);
    const double wr = nr * pr / den;
    const double wi = 0.0 - nr * pq / den;
    const double m = std::min(1.0, d * 20.0);
    const double hr = 1.0 + (wr - 1.0) * m;
    const double hi = wi * m;
    return 10.0 * std::log10(std::max(hr * hr + hi * hi, 1e-20));
}

// coefficients of one TPT SVF stage (stage() in the JSFX)
struct Stage {
    double k = 1.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    void set(double g, double q) noexcept
    {
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

// state of one stage (svf() in the JSFX)
struct Svf {
    double ic1 = 0.0, ic2 = 0.0;
    double run(double x, const Stage& c, bool lp) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return lp ? v2 : x - c.k * v1 - v2;
    }
    void flush() noexcept
    {
        if (std::fabs(ic1) < 0.000000000000001) ic1 = 0.0;
        if (std::fabs(ic2) < 0.000000000000001) ic2 = 0.0;
    }
};

class DjFilterProcessor {
public:
    // @init
    void init(double sr) noexcept
    {
        fSr = sr;
        fKs = 1.0 - std::exp(-1.0 / (0.02 * sr));
        fSnap = true;
        for (Svf* s : { &fLpL1, &fLpL2, &fLpR1, &fLpR2, &fHpL1, &fHpL2, &fHpR1, &fHpR2 })
            *s = Svf();
    }

    // @slider
    void setFilter(double v) noexcept
    {
        fTgt = v / 100.0;
        if (fSnap)
        {
            fPos = fTgt;
            coefs();
            fSnap = false;
        }
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        // @block
        for (Svf* s : { &fLpL1, &fLpL2, &fLpR1, &fLpR2, &fHpL1, &fHpL2, &fHpR1, &fHpR2 })
            s->flush();

        for (uint32_t i = 0; i < frames; ++i)
        {
            if (fPos != fTgt)
            {
                fPos += (fTgt - fPos) * fKs;
                if (std::fabs(fTgt - fPos) < 0.0001)
                    fPos = fTgt;
                coefs();
            }
            double l = inL[i], r = inR[i];
            const double loL = fLpL2.run(fLpL1.run(l, fLp1, true), fLp2, true);
            const double loR = fLpR2.run(fLpR1.run(r, fLp1, true), fLp2, true);
            const double hiL = fHpL2.run(fHpL1.run(l, fHp1, false), fHp2, false);
            const double hiR = fHpR2.run(fHpR1.run(r, fHp1, false), fHp2, false);
            if (fMix > 0.0)
            {
                l += ((fPos < 0.0 ? loL : hiL) - l) * fMix;
                r += ((fPos < 0.0 ? loR : hiR) - r) * fMix;
            }
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    void coefs() noexcept
    {
        double d = std::max(0.0, -fPos);
        double g = std::tan(kPi * std::min(20000.0 * std::pow(60.0 / 20000.0, d), fSr * 0.45) / fSr);
        fLp1.set(g, 0.5412);
        fLp2.set(g, 1.3066 + 2.5 * d);
        d = std::max(0.0, fPos);
        g = std::tan(kPi * std::min(20.0 * std::pow(10000.0 / 20.0, d), fSr * 0.45) / fSr);
        fHp1.set(g, 0.5412);
        fHp2.set(g, 1.3066 + 2.5 * d);
        fMix = std::min(1.0, std::fabs(fPos) * 20.0);
    }

    double fSr = 48000.0, fKs = 0.0;
    double fPos = 0.0, fTgt = 0.0, fMix = 0.0;
    bool fSnap = true;
    Stage fLp1, fLp2, fHp1, fHp2;
    Svf fLpL1, fLpL2, fLpR1, fLpR2, fHpL1, fHpL2, fHpR1, fHpR2;
};

} // namespace nulldjfilter
