/*
 * NULL JSFX - Harmonizer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/harmonizer.jsfx, computed in double like EEL2: the mono sum
 * feeds a 0.2 s line read by two crossfading two-tap pitch shifters (voice 1 and 2, detuned
 * and delayed), panned apart by Spread and blended with an equal-power dry/wet Mix.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullharmonizer {

class HarmonizerProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fWin = std::floor(sr * 0.05);
        fSize = int(std::ceil(sr * 0.2) + fWin);
        fBuf.assign(size_t(fSize + 2), 0.0); // + the never-written memory after the JSFX's line
        fW = 0;
        fPh1 = fPh2 = 0.0;
    }

    // @slider; s[0..7] = slider1..slider8
    void setParams(const double* s)
    {
        fR1 = std::pow(2.0, (s[0] + s[4] / 100.0) / 12.0);
        fR2 = std::pow(2.0, (s[1] - s[4] / 100.0) / 12.0);
        fG1 = std::pow(10.0, s[2] / 20.0);
        fG2 = std::pow(10.0, s[3] / 20.0);
        fBase1 = s[5] * fSr / 1000.0;
        fBase2 = s[5] * 1.37 * fSr / 1000.0;
        fSp = s[6] / 100.0;
        fDry = std::cos(s[7] / 100.0 * M_PI / 2.0);
        fWet = std::sin(s[7] / 100.0 * M_PI / 2.0);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            fBuf[size_t(fW)] = (xl + xr) * 0.5;
            const double v1 = voice(fPh1, fR1, fBase1) * fG1;
            const double v2 = voice(fPh2, fR2, fBase2) * fG2;
            if (++fW >= fSize)
                fW = 0;
            const double p1 = M_PI / 4.0 * (1.0 - fSp);
            const double p2 = M_PI / 2.0 - p1;
            const double wl = v1 * std::cos(p1) + v2 * std::cos(p2);
            const double wr = v1 * std::sin(p1) + v2 * std::sin(p2);
            outL[n] = static_cast<T>(xl * fDry + wl * fWet);
            outR[n] = static_cast<T>(xr * fDry + wr * fWet);
        }
    }

private:
    double tap(double d) const noexcept
    {
        double rp = double(fW) - d;
        if (rp < 0.0)
            rp += double(fSize);
        const double i = std::floor(rp), f = rp - i;
        double i2 = i + 1.0;
        if (i2 >= double(fSize))
            i2 = 0.0;
        return fBuf[size_t(i)] + (fBuf[size_t(i2)] - fBuf[size_t(i)]) * f;
    }

    // two-tap crossfading pitch shifter (the JSFX's voice())
    double voice(double& ph, double ratio, double base) const noexcept
    {
        ph += (1.0 - ratio) / fWin;
        if (ph >= 1.0)
            ph -= 1.0;
        if (ph < 0.0)
            ph += 1.0;
        double p2 = ph + 0.5;
        if (p2 >= 1.0)
            p2 -= 1.0;
        const double g = 0.5 - 0.5 * std::cos(2.0 * M_PI * ph);
        return tap(base + 1.0 + ph * fWin) * g + tap(base + 1.0 + p2 * fWin) * (1.0 - g);
    }

    double fSr = 48000.0, fWin = 1.0;
    std::vector<double> fBuf;
    int fSize = 1, fW = 0;
    double fPh1 = 0.0, fPh2 = 0.0;
    double fR1 = 1.0, fR2 = 1.0, fG1 = 1.0, fG2 = 1.0, fBase1 = 0.0, fBase2 = 0.0, fSp = 0.0, fDry = 1.0, fWet = 0.0;
};

} // namespace nullharmonizer
