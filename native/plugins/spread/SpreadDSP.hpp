/*
 * NULL JSFX - Spread, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/spread.jsfx, computed in double like EEL2 and in the same
 * order: the mid signal is high-passed (one pole, 250 Hz), delayed by two taps (11.3 ms
 * x 0.6 + 17.9 ms x 0.4) and added to the left / subtracted from the right at amount*0.7.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullspread {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

class SpreadProcessor {
public:
    void setSampleRate(double sr)
    {
        fSr = sr;
        fBufSize = std::ceil(sr * 0.05);
        fBuf.assign(size_t(fBufSize), 0.0);
        update();
        reset();
    }

    void setAmount(double v) noexcept
    {
        fValue = v;
        update();
    }

    // JSFX @init
    void reset() noexcept
    {
        std::fill(fBuf.begin(), fBuf.end(), 0.0);
        fWpos = 0.0;
        fHpX = fHpY = 0.0;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i) {
            double x0 = inL[i], x1 = inR[i];
            const double mid = (x0 + x1) * 0.5;
            fHpY = fHpC * (fHpY + mid - fHpX);
            fHpX = mid;
            fBuf[size_t(fWpos)] = fHpY;
            const double d = tap(11.3) * 0.6 + tap(17.9) * 0.4;
            fWpos += 1.0;
            if (fWpos >= fBufSize)
                fWpos = 0.0;
            x0 += d * fWidth;
            x1 -= d * fWidth;
            outL[i] = static_cast<T>(x0);
            outR[i] = static_cast<T>(x1);
        }
    }

private:
    // the JSFX @slider section
    void update() noexcept
    {
        const double amount = fValue / 100.0;
        fHpC = std::exp(-2.0 * kPi * 250.0 / fSr);
        fWidth = amount * 0.7;
    }

    double tap(double ms) const noexcept
    {
        double rp = fWpos - std::floor(ms * fSr / 1000.0);
        if (rp < 0.0)
            rp += fBufSize;
        return fBuf[size_t(rp)];
    }

    double fSr = 48000.0, fBufSize = 2400.0, fValue = 50.0;
    double fHpC = 0.0, fWidth = 0.35;
    std::vector<double> fBuf;
    double fWpos = 0.0, fHpX = 0.0, fHpY = 0.0;
};

} // namespace nullspread
