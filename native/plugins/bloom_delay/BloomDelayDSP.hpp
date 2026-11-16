/*
 * NULL JSFX - Bloom Delay, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/bloom_delay.jsfx, computed in double in the EEL2 order: a
 * 4 s stereo delay line read with linear interpolation (time from Time or the tempo, glided,
 * swept by a 0.35 Hz LFO), then in the loop a damping low-pass, a 30 Hz high-pass and four
 * allpass diffusers per side blended in by Bloom, the feedback through a soft tanh curve, and
 * the wet signal narrowed by Width.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullbloom {

class BloomDelayProcessor {
public:
    // @init (allocates)
    void setSampleRate(double sr)
    {
        fSr = sr;
        fN = int(std::ceil(sr * 4.0)) + 8;
        fBl.assign(size_t(fN), 0.0);
        fBr.assign(size_t(fN), 0.0);
        static const double kMs[8] = { 4.77, 3.59, 12.73, 9.30, 5.21, 3.97, 11.83, 8.71 };
        for (int k = 0; k < 8; ++k)
        {
            Ap& a = fAp[k];
            a.n = int(std::max(1.0, std::floor(kMs[k] * 0.001 * sr + 0.5)));
            a.p = 0;
            a.b.assign(size_t(a.n), 0.0);
        }
        fWp = 0;
        fDt = -1.0;
        fGlide = 1.0 - std::exp(-1.0 / (0.08 * sr));
        fHpc = 1.0 - std::exp(-2.0 * M_PI * 30.0 / sr);
        fLfo = 0.0;
        fLpl = fLpr = fHpl = fHpr = 0.0;
        fOl = fOr = 0.0;
    }

    // @slider, v = slider1..slider9 as the JSFX holds them
    void setParams(const double* v)
    {
        fSync = v[1];
        fNote = beats(v[1]);
        fMs = v[0];
        fFb = v[2] / 100.0;
        fBloom = v[3] / 100.0;
        fDepth = v[4] / 100.0 * 0.002 * fSr;
        fLpc = 1.0 - std::exp(-2.0 * M_PI * std::min(v[5], 0.45 * fSr) / fSr);
        fW = v[6] / 100.0;
        fMix = v[7] / 100.0;
        fOut = std::pow(10.0, v[8] / 20.0);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            double t = fSync != 0.0 ? fNote * 60.0 / (tempo > 0.0 ? tempo : 120.0) * fSr : fMs * 0.001 * fSr;
            t = std::min(fN - 2.0 * fDepth - 4.0, std::max(1.0, t));
            if (fDt < 0.0)
                fDt = t;
            else
                fDt += (t - fDt) * fGlide;
            fLfo += 0.35 / fSr;
            if (fLfo >= 1.0)
                fLfo -= 1.0;
            const double yl = read(fBl, fDt + fDepth * (1.0 + std::sin(2.0 * M_PI * fLfo)));
            const double yr = read(fBr, fDt + fDepth * (1.0 + std::cos(2.0 * M_PI * fLfo)));
            fLpl += (yl - fLpl) * fLpc;
            fLpr += (yr - fLpr) * fLpc;
            fHpl += (fLpl - fHpl) * fHpc;
            fHpr += (fLpr - fHpr) * fHpc;
            const double fl = fLpl - fHpl, fr = fLpr - fHpr;
            const double dl = ap(fAp[3], ap(fAp[2], ap(fAp[1], ap(fAp[0], fl))));
            const double dr = ap(fAp[7], ap(fAp[6], ap(fAp[5], ap(fAp[4], fr))));
            fOl = fl + (dl - fl) * fBloom;
            fOr = fr + (dr - fr) * fBloom;
            fBl[size_t(fWp)] = xl + sat(fOl * fFb);
            fBr[size_t(fWp)] = xr + sat(fOr * fFb);
            if (++fWp >= fN)
                fWp = 0;
            const double mm = (fOl + fOr) * 0.5;
            const double wl = mm + (fOl - mm) * fW, wr = mm + (fOr - mm) * fW;
            outL[i] = static_cast<T>((xl + (wl - xl) * fMix) * fOut);
            outR[i] = static_cast<T>((xr + (wr - xr) * fMix) * fOut);
        }
    }

private:
    struct Ap {
        std::vector<double> b;
        int n = 1, p = 0;
    };

    // bd_beats: the note length (beats) of Sync 1 to 7
    static double beats(double v) noexcept
    {
        return v == 1.0 ? 0.25 : v == 2.0 ? 0.5 : v == 3.0 ? 0.75 : v == 4.0 ? 1.0 : v == 5.0 ? 1.5 : v == 6.0 ? 2.0 : 4.0;
    }

    static double sat(double x) noexcept
    {
        const double e = std::exp(2.0 * std::max(-20.0, std::min(20.0, x)));
        return (e - 1.0) / (e + 1.0);
    }

    static double ap(Ap& a, double x) noexcept
    {
        const double z = a.b[size_t(a.p)];
        const double w = x - 0.6 * z;
        a.b[size_t(a.p)] = w;
        if (++a.p >= a.n)
            a.p = 0;
        return z + 0.6 * w;
    }

    double read(const std::vector<double>& b, double d) const noexcept
    {
        double rp = fWp - d;
        if (rp < 0.0)
            rp += fN;
        if (rp >= fN) // a tiny negative rp can round up to fN
            rp -= fN;
        const int i0 = int(std::floor(rp));
        const double fr = rp - i0;
        int i1 = i0 + 1;
        if (i1 >= fN)
            i1 = 0;
        return b[size_t(i0)] + (b[size_t(i1)] - b[size_t(i0)]) * fr;
    }

    double fSr = 48000.0;
    int fN = 0, fWp = 0;
    std::vector<double> fBl, fBr;
    Ap fAp[8];
    double fDt = -1.0, fGlide = 0.0, fHpc = 0.0, fLfo = 0.0;
    double fLpl = 0.0, fLpr = 0.0, fHpl = 0.0, fHpr = 0.0, fOl = 0.0, fOr = 0.0;
    double fSync = 0.0, fNote = 4.0, fMs = 375.0, fFb = 0.45, fBloom = 0.5, fDepth = 0.0, fLpc = 1.0, fW = 1.0,
           fMix = 0.35, fOut = 1.0;
};

} // namespace nullbloom
