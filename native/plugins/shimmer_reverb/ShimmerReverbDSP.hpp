/*
 * NULL JSFX - Shimmer Reverb, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/shimmer_reverb.jsfx, computed in double like EEL2: a 4-line
 * feedback delay network (Hadamard mixing, one-pole damping in the loop) whose tail runs
 * through a two-tap pitch shifter and back in, so every round trip climbs another interval.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullshimmer {

// the pitch interval in semitones for the Pitch option
inline double semitones(int option) noexcept
{
    return option == 0 ? 12.0 : option == 1 ? 7.0 : option == 2 ? 19.0 : option == 3 ? 24.0 : -12.0;
}

class ShimmerReverbProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fMaxLen = int(std::ceil(sr * 0.2));
        fWin = std::floor(sr * 0.08);
        fPsSize = int(fWin) + 4;
        // d1..d4 then the pitch-shifter line, like the JSFX's memory (+ never-written memory)
        fMem.assign(size_t(4 * fMaxLen + fPsSize + 2), 0.0);
        fW = fP = 0;
        fPh = 0.0;
        fF1 = fF2 = fF3 = fF4 = 0.0;
    }

    // @slider; s[0..5] = slider1..slider6
    void setParams(const double* s)
    {
        const double sz = 0.4 + s[0] / 100.0 * 0.6;
        fL1 = int(std::floor(fSr * 0.0731 * sz));
        fL2 = int(std::floor(fSr * 0.0857 * sz));
        fL3 = int(std::floor(fSr * 0.0997 * sz));
        fL4 = int(std::floor(fSr * 0.1129 * sz));
        const double avg = double(fL1 + fL2 + fL3 + fL4) / 4.0 / fSr;
        fG = std::pow(10.0, -3.0 * avg / s[1]);
        fShimmer = s[2] / 100.0;
        const double ratio = std::pow(2.0, semitones(int(std::lround(s[3]))) / 12.0);
        fPhInc = (1.0 - ratio) / fWin;
        fDamp = std::exp(-2.0 * M_PI * s[4] / fSr);
        fMix = s[5] / 100.0;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        double* const d1 = fMem.data();
        double* const d2 = d1 + fMaxLen;
        double* const d3 = d2 + fMaxLen;
        double* const d4 = d3 + fMaxLen;
        double* const ps = d4 + fMaxLen;
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            const double o1 = line(d1, fL1), o2 = line(d2, fL2), o3 = line(d3, fL3), o4 = line(d4, fL4);
            fF1 = (1.0 - fDamp) * o1 + fDamp * fF1;
            fF2 = (1.0 - fDamp) * o2 + fDamp * fF2;
            fF3 = (1.0 - fDamp) * o3 + fDamp * fF3;
            fF4 = (1.0 - fDamp) * o4 + fDamp * fF4;

            ps[fP] = (fF1 + fF2 + fF3 + fF4) * 0.25;
            fPh += fPhInc;
            if (fPh >= 1.0)
                fPh -= 1.0;
            if (fPh < 0.0)
                fPh += 1.0;
            double ph2 = fPh + 0.5;
            if (ph2 >= 1.0)
                ph2 -= 1.0;
            const double gw = 0.5 - 0.5 * std::cos(2.0 * M_PI * fPh);
            const double shim = psTap(ps, 1.0 + fPh * fWin) * gw + psTap(ps, 1.0 + ph2 * fWin) * (1.0 - gw);
            if (++fP >= fPsSize)
                fP = 0;

            const double h1 = (fF1 + fF2 + fF3 + fF4) * 0.5, h2 = (fF1 - fF2 + fF3 - fF4) * 0.5;
            const double h3 = (fF1 + fF2 - fF3 - fF4) * 0.5, h4 = (fF1 - fF2 - fF3 + fF4) * 0.5;
            const double s = shim * fShimmer;
            const double k = fG * (1.0 - fShimmer * 0.5);
            d1[fW] = xl + h1 * k + s * fG;
            d2[fW] = xr + h2 * k + s * fG;
            d3[fW] = xl + h3 * k;
            d4[fW] = xr + h4 * k;
            if (++fW >= fMaxLen)
                fW = 0;

            const double wl = (o1 + o3) * 0.35, wr = (o2 + o4) * 0.35;
            outL[n] = static_cast<T>(xl * (1.0 - fMix) + wl * fMix);
            outR[n] = static_cast<T>(xr * (1.0 - fMix) + wr * fMix);
        }
    }

private:
    double line(const double* buf, int len) const noexcept
    {
        int rp = fW - len;
        if (rp < 0)
            rp += fMaxLen;
        return buf[size_t(rp)];
    }

    double psTap(const double* ps, double d) const noexcept
    {
        double rp = double(fP) - d;
        if (rp < 0.0)
            rp += double(fPsSize);
        const double i = std::floor(rp), f = rp - i;
        double i2 = i + 1.0;
        if (i2 >= double(fPsSize))
            i2 = 0.0;
        return ps[size_t(i)] + (ps[size_t(i2)] - ps[size_t(i)]) * f;
    }

    double fSr = 48000.0, fWin = 1.0;
    std::vector<double> fMem;
    int fMaxLen = 1, fPsSize = 1, fW = 0, fP = 0;
    int fL1 = 1, fL2 = 1, fL3 = 1, fL4 = 1;
    double fPh = 0.0, fF1 = 0.0, fF2 = 0.0, fF3 = 0.0, fF4 = 0.0;
    double fG = 0.0, fShimmer = 0.0, fPhInc = 0.0, fDamp = 0.0, fMix = 0.0;
};

} // namespace nullshimmer
