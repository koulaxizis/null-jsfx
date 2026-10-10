/*
 * NULL JSFX - Stretch, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/stretch.jsfx (computed in double, like EEL2): a two-grain
 * delay-line pitch shifter with Hann crossfades. Negative values drop the pitch (up to an
 * octave) with long grains, positive values raise it with short ones; 0 is untouched.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullstretch {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    double ratio, win, phInc, wet;
};

// @slider (maxWin = ceil(srate * 0.25))
inline Params paramsFor(double slider, double srate) noexcept
{
    const double v = slider / 100.0;
    const double ratio = std::pow(2.0, v);
    double win = std::floor(srate * (v < 0.0 ? 0.04 - v * 0.2 : 0.04 - v * 0.015));
    win = std::fmin(win, std::ceil(srate * 0.25));
    return { ratio, win, (1.0 - ratio) / win, std::fmin(1.0, std::fabs(v) * 4.0) };
}

class StretchProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fBufSize = int(std::ceil(sr * 0.25)) + 4;
        fBufL.assign(size_t(fBufSize), 0.0);
        fBufR.assign(size_t(fBufSize), 0.0);
        fWpos = 0;
        fPh = 0.0;
        fP = paramsFor(fSlider, sr); // @slider runs again after @init
    }

    void setAmount(double v) noexcept { fSlider = v; fP = paramsFor(v, fSr); } // @slider

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const double win = fP.win, phInc = fP.phInc, wet = fP.wet;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double s0 = inL[i], s1 = inR[i];
            fBufL[size_t(fWpos)] = s0;
            fBufR[size_t(fWpos)] = s1;
            fPh += phInc;
            if (fPh >= 1.0) fPh -= 1.0;
            if (fPh < 0.0) fPh += 1.0;
            double ph2 = fPh + 0.5;
            if (ph2 >= 1.0) ph2 -= 1.0;
            const double g1 = 0.5 - 0.5 * std::cos(2.0 * kPi * fPh);
            const double g2 = 1.0 - g1;
            const double d1 = 1.0 + fPh * win, d2 = 1.0 + ph2 * win;
            const double sl = tap(fBufL, d1) * g1 + tap(fBufL, d2) * g2;
            const double sr = tap(fBufR, d1) * g1 + tap(fBufR, d2) * g2;
            fWpos += 1;
            if (fWpos >= fBufSize) fWpos = 0;
            outL[i] = static_cast<T>(s0 * (1.0 - wet) + sl * wet);
            outR[i] = static_cast<T>(s1 * (1.0 - wet) + sr * wet);
        }
    }

private:
    double tap(const std::vector<double>& buf, double delay) const noexcept
    {
        double rp = fWpos - delay;
        if (rp < 0.0) rp += fBufSize;
        const double i = std::floor(rp);
        const double f = rp - i;
        int i2 = int(i) + 1;
        if (i2 >= fBufSize) i2 = 0;
        const double b1 = buf[size_t(i)];
        return b1 + (buf[size_t(i2)] - b1) * f;
    }

    double fSr = 48000.0, fPh = 0.0, fSlider = 0.0;
    int fBufSize = 4, fWpos = 0;
    std::vector<double> fBufL, fBufR;
    Params fP { 1.0, 1.0, 0.0, 0.0 };
};

} // namespace nullstretch
