/*
 * NULL JSFX - Doubler, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/doubler.jsfx, computed in double like EEL2 and in the same
 * order: the mono sum goes into a 50 ms buffer, two interpolated taps (14 ms + 6*amount
 * with a 0.13 Hz +-2 ms drift, 21 ms + 8*amount with a 0.21 Hz +-2.5 ms drift) are added to
 * the left and right channels at amount*0.8 and the result is scaled by 1/(1 + level/2).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulldoubler {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    double amount, level, dry;
};

// the JSFX @slider section
inline Params paramsForAmount(double v) noexcept
{
    const double amount = v / 100.0;
    const double level = amount * 0.8;
    return { amount, level, 1.0 / (1.0 + level * 0.5) };
}

class DoublerProcessor {
public:
    void setSampleRate(double sr)
    {
        fSr = sr;
        fBufSize = std::ceil(sr * 0.05);
        fBuf.assign(size_t(fBufSize), 0.0);
        reset();
    }

    void setAmount(double v) noexcept { fP = paramsForAmount(v); }

    // JSFX @init
    void reset() noexcept
    {
        std::fill(fBuf.begin(), fBuf.end(), 0.0);
        fWpos = 0.0;
        fPh1 = 0.0;
        fPh2 = 0.37;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i) {
            const double x0 = inL[i], x1 = inR[i];
            const double mono = (x0 + x1) * 0.5;
            fBuf[size_t(fWpos)] = mono;
            fPh1 += 0.13 / fSr;
            if (fPh1 >= 1.0)
                fPh1 -= 1.0;
            fPh2 += 0.21 / fSr;
            if (fPh2 >= 1.0)
                fPh2 -= 1.0;
            const double d1 = tap(14.0 + 2.0 * std::sin(2.0 * kPi * fPh1) + fP.amount * 6.0);
            const double d2 = tap(21.0 + 2.5 * std::sin(2.0 * kPi * fPh2) + fP.amount * 8.0);
            fWpos += 1.0;
            if (fWpos >= fBufSize)
                fWpos = 0.0;

            outL[i] = static_cast<T>((x0 + d1 * fP.level) * fP.dry);
            outR[i] = static_cast<T>((x1 + d2 * fP.level) * fP.dry);
        }
    }

private:
    double tap(double delayMs) const noexcept
    {
        double rp = fWpos - delayMs * fSr / 1000.0;
        if (rp < 0.0)
            rp += fBufSize;
        const double ii = std::floor(rp);
        const double f = rp - ii;
        double i2 = ii + 1.0;
        if (i2 >= fBufSize)
            i2 = 0.0;
        const double a = fBuf[size_t(ii)];
        return a + (fBuf[size_t(i2)] - a) * f;
    }

    double fSr = 48000.0, fBufSize = 2400.0;
    Params fP = paramsForAmount(40.0);
    std::vector<double> fBuf;
    double fWpos = 0.0, fPh1 = 0.0, fPh2 = 0.37;
};

} // namespace nulldoubler
