/*
 * NULL JSFX - Phase Distortion, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/phase_distortion.jsfx (computed in double, like EEL2): a very
 * short delay whose time follows the track itself or the sidechain, low-passed by Tone and
 * offset by Bias; the dry signal is delayed by the same centre time.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullphasedist {

struct Params {
    double depth, tc, bias, sp, mix, out;
    bool key;
};

// @slider; the values on the JSFX grid
inline Params paramsFor(double drive, double tone, double bias, double spread, double source, double mix,
                        double output, double sr) noexcept
{
    Params p;
    p.depth = std::pow(drive / 100.0, 2.0) * 0.003 * sr;
    p.tc = 1.0 - std::exp(-2.0 * M_PI * std::min(tone, 0.45 * sr) / sr);
    p.bias = bias / 100.0;
    p.sp = 1.0 - 2.0 * spread / 100.0;
    p.key = source >= 0.5;
    p.mix = mix / 100.0;
    p.out = std::pow(10.0, output / 20.0);
    return p;
}

// the display: a 440 Hz sine at -6 dB through the effect at input phase ph (Tone, Spread and
// Mix left out, and the centre delay, which the dry signal shares)
inline double shape(const Params& p, double ph, double sr) noexcept
{
    return std::sin(ph - 2.0 * M_PI * 440.0 * p.depth * std::min(1.0, std::max(-1.0, 0.5 * std::sin(ph) + p.bias)) / sr);
}

class PhaseDistortionProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fN = int(std::ceil(sr * 0.008)) + 8;
        fBl.assign(size_t(fN), 0.0);
        fBr.assign(size_t(fN), 0.0);
        fWp = 0;
        fMl = fMr = 0.0;
    }

    void setParams(const Params& p) noexcept { fP = p; }

    template <typename T>
    void process(const T* inL, const T* inR, const T* scL, const T* scR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Params p = fP;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            const double kl = p.key ? double(scL[i]) : xl;
            const double kr = p.key ? double(scR[i]) : xr;
            fBl[size_t(fWp)] = xl;
            fBr[size_t(fWp)] = xr;
            fMl += (kl - fMl) * p.tc;
            fMr += (kr - fMr) * p.tc;
            const double dl = 1.0 + p.depth * (1.0 + std::min(1.0, std::max(-1.0, fMl + p.bias)));
            const double dr = 1.0 + p.depth * (1.0 + std::min(1.0, std::max(-1.0, fMr * p.sp + p.bias)));
            const double c = 1.0 + p.depth;
            const double wl = read(fBl, dl), wr = read(fBr, dr);
            const double yl = read(fBl, c), yr = read(fBr, c);
            fWp += 1;
            if (fWp >= fN)
                fWp = 0;
            outL[i] = static_cast<T>((yl + (wl - yl) * p.mix) * p.out);
            outR[i] = static_cast<T>((yr + (wr - yr) * p.mix) * p.out);
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

    Params fP = paramsFor(50.0, 2000.0, 0.0, 0.0, 0.0, 100.0, 0.0, 44100.0);
    std::vector<double> fBl, fBr;
    int fN = 1, fWp = 0;
    double fMl = 0.0, fMr = 0.0;
};

} // namespace nullphasedist
