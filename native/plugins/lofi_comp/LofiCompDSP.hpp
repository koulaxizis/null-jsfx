/*
 * NULL JSFX - Lo-Fi Comp, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/lofi_comp.jsfx (computed in double, like EEL2): a pumping
 * compressor, a slow pitch drift (two LFOs on a short delay) and an old sampler (low-pass,
 * sample-and-hold at Sample Rate, quantising to Bits).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulllofi {

struct Params {
    double thr, ratio, mu, rel, step, lpc, q, depth, mix, out;
    bool on;
};

// @slider; the values on the JSFX grid
inline Params paramsFor(const double* v, double sr) noexcept
{
    Params p;
    const double s = v[0] / 100.0;
    p.thr = -10.0 - 30.0 * s;
    p.ratio = 2.0 + 18.0 * s;
    p.mu = std::pow(10.0, -p.thr * (1.0 - 1.0 / p.ratio) * (0.35 + 0.35 * s) / 20.0);
    p.rel = 1.0 - std::exp(-1.0 / (v[1] * 0.001 * sr));
    p.on = v[2] < sr;
    p.step = v[2] / sr;
    p.lpc = 1.0 - std::exp(-2.0 * M_PI * std::min(0.45 * v[2], 0.45 * sr) / sr);
    p.q = std::pow(2.0, v[3] - 1.0);
    p.depth = v[4] / 100.0 * 0.002 * sr;
    p.mix = v[5] / 100.0;
    p.out = std::pow(10.0, v[6] / 20.0);
    return p;
}

// the display: the level out (dB) for a steady level in (dB), make-up included
inline double curveDb(const Params& p, double x) noexcept
{
    return (x > p.thr ? p.thr + (x - p.thr) / p.ratio : x) + 20.0 * std::log10(p.mu);
}

class LofiCompProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fN = int(std::ceil(sr * 0.01)) + 4;
        fBl.assign(size_t(fN), 0.0);
        fBr.assign(size_t(fN), 0.0);
        fWp = 0;
        fAtt = 1.0 - std::exp(-1.0 / (0.001 * sr));
        fE = 0.0;
        fP1 = fP2 = 0.0;
        fPh = 1.0;
        fHl = fHr = fAl = fAr = 0.0;
    }

    void setParams(const Params& p) noexcept { fP = p; }
    double env() const noexcept { return fE; }
    double grDb() const noexcept
    {
        return std::max(0.0, (20.0 * std::log10(std::max(fE, 0.000001)) - fP.thr) * (1.0 - 1.0 / fP.ratio));
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Params p = fP;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            const double in = std::max(std::fabs(xl), std::fabs(xr));
            fE += (in - fE) * (in > fE ? fAtt : p.rel);
            const double over = 20.0 * std::log10(std::max(fE, 0.000001)) - p.thr;
            const double g = (over > 0.0 ? std::pow(10.0, -over * (1.0 - 1.0 / p.ratio) / 20.0) : 1.0) * p.mu;
            fBl[size_t(fWp)] = xl * g;
            fBr[size_t(fWp)] = xr * g;
            fP1 += 0.6 / fSr;
            if (fP1 >= 1.0)
                fP1 -= 1.0;
            fP2 += 0.17 / fSr;
            if (fP2 >= 1.0)
                fP2 -= 1.0;
            const double d = p.depth * (1.0 + 0.6 * std::sin(2.0 * M_PI * fP1) + 0.4 * std::sin(2.0 * M_PI * fP2));
            double l = read(fBl, d), r = read(fBr, d);
            fWp += 1;
            if (fWp >= fN)
                fWp = 0;
            if (p.on)
            {
                fAl += (l - fAl) * p.lpc;
                fAr += (r - fAr) * p.lpc;
                fPh += p.step;
                if (fPh >= 1.0)
                {
                    fPh -= 1.0;
                    fHl = fAl;
                    fHr = fAr;
                }
                l = fHl;
                r = fHr;
            }
            l = std::floor(l * p.q + 0.5) / p.q;
            r = std::floor(r * p.q + 0.5) / p.q;
            outL[i] = static_cast<T>((xl + (l - xl) * p.mix) * p.out);
            outR[i] = static_cast<T>((xr + (r - xr) * p.mix) * p.out);
        }
    }

private:
    double read(const std::vector<double>& b, double d) const noexcept
    {
        double rp = double(fWp) - d;
        if (rp < 0.0)
            rp += double(fN);
        const int i0 = int(std::floor(rp));
        const double fr = rp - double(i0);
        int i1 = i0 + 1;
        if (i1 >= fN)
            i1 = 0;
        return b[size_t(i0)] + (b[size_t(i1)] - b[size_t(i0)]) * fr;
    }

    Params fP {};
    double fSr = 44100.0, fAtt = 0.0, fE = 0.0, fP1 = 0.0, fP2 = 0.0, fPh = 1.0;
    double fHl = 0.0, fHr = 0.0, fAl = 0.0, fAr = 0.0;
    std::vector<double> fBl, fBr;
    int fN = 1, fWp = 0;
};

} // namespace nulllofi
