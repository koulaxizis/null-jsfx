/*
 * NULL JSFX - Warped Delay, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/warped_delay.jsfx, computed in double like EEL2: a stereo
 * delay (up to 1 s) whose time an LFO wobbles (the right channel a quarter cycle behind), with
 * a biquad (off, LP, HP, BP) and a soft saturator inside the feedback loop. Both lines sit in
 * one block like the JSFX's memory, so an edge read past the left line lands in the right one.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullwarped {

class WarpedDelayProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 1.1));
        fMem.assign(size_t(2 * fSize + 2), 0.0);
        fW = 0;
        fPhase = 0.0;
        fL = Biquad();
        fR = Biquad();
    }

    // @slider; s[0..7] = slider1..slider8
    void setParams(const double* s)
    {
        fDelMs = s[0];
        fRate = s[1];
        fDepth = std::min(s[2], s[0] - 0.5);
        fFeedback = s[3] / 100.0;
        fDist = s[4] / 100.0;
        const int mode = int(std::lround(s[5]));
        fMix = s[7] / 100.0;

        const double w = 2.0 * M_PI * std::min(s[6], fSr * 0.45) / fSr;
        const double cs = std::cos(w);
        const double alpha = std::sin(w) / (2.0 * 0.707);
        const double n = 1.0 / (1.0 + alpha);
        if (mode == 1)
        {
            fB0 = (1.0 - cs) * 0.5 * n; fB1 = (1.0 - cs) * n; fB2 = fB0;
        }
        else if (mode == 2)
        {
            fB0 = (1.0 + cs) * 0.5 * n; fB1 = -(1.0 + cs) * n; fB2 = fB0;
        }
        else if (mode == 3)
        {
            fB0 = alpha * n; fB1 = 0.0; fB2 = -alpha * n;
        }
        else
        {
            fB0 = 1.0; fB1 = fB2 = 0.0;
        }
        if (mode == 0)
            fA1 = fA2 = 0.0;
        else
        {
            fA1 = -2.0 * cs * n;
            fA2 = (1.0 - alpha) * n;
        }
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        double* const bl = fMem.data();
        double* const br = bl + fSize;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            fPhase += 2.0 * M_PI * fRate / fSr;
            if (fPhase >= 2.0 * M_PI)
                fPhase -= 2.0 * M_PI;
            const double dl = std::max(1.0, (fDelMs + std::sin(fPhase) * fDepth) * fSr / 1000.0);
            const double dr = std::max(1.0, (fDelMs + std::cos(fPhase) * fDepth) * fSr / 1000.0);

            double wl = biquad(fL, tap(bl, dl));
            double wr = biquad(fR, tap(br, dr));
            wl = warm(wl);
            wr = warm(wr);

            bl[fW] = xl + wl * fFeedback;
            br[fW] = xr + wr * fFeedback;
            if (++fW >= fSize)
                fW = 0;

            outL[i] = static_cast<T>(xl * (1.0 - fMix) + wl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + wr * fMix);
        }
    }

private:
    struct Biquad {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    };

    double tap(const double* buf, double delay) const noexcept
    {
        double rp = double(fW) - delay;
        if (rp < 0.0)
            rp += double(fSize);
        const double i = std::floor(rp), fr = rp - i;
        double i2 = i + 1.0;
        if (i2 >= double(fSize))
            i2 = 0.0;
        return buf[size_t(i)] + (buf[size_t(i2)] - buf[size_t(i)]) * fr;
    }

    double biquad(Biquad& s, double x) const noexcept
    {
        const double y = fB0 * x + fB1 * s.x1 + fB2 * s.x2 - fA1 * s.y1 - fA2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    double warm(double x) const noexcept
    {
        double d = x * (1.0 + fDist * 3.0);
        d = std::max(-3.0, std::min(3.0, d));
        const double d2 = d * d;
        return d * (27.0 + d2) / (27.0 + 9.0 * d2) / (1.0 + fDist);
    }

    double fSr = 48000.0;
    std::vector<double> fMem;
    int fSize = 1, fW = 0;
    double fPhase = 0.0;
    Biquad fL, fR;
    double fDelMs = 150.0, fRate = 2.0, fDepth = 10.0, fFeedback = 0.4, fDist = 0.3, fMix = 0.5;
    double fB0 = 1.0, fB1 = 0.0, fB2 = 0.0, fA1 = 0.0, fA2 = 0.0;
};

} // namespace nullwarped
