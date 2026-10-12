/*
 * NULL JSFX - Chorus, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/chorus.jsfx, computed in double like EEL2: a sine LFO sweeps
 * a stereo delay line between 5 ms and 5 ms + Depth (linear interpolation), blended with the
 * dry signal (Mix).
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullchorus {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI

// the delay time in ms at LFO value lfo (-1..1)
inline double delayMsAt(double depthMs, double lfo) noexcept
{
    const double mod = depthMs * 0.5;
    return 5.0 + mod + lfo * mod;
}

class ChorusProcessor {
public:
    // @init: 100 ms buffers, cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 0.1));
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fPhase = 0.0;
    }

    // @slider
    void setParams(double rateHz, double depthMs, double mixPct)
    {
        fDepth = depthMs;
        fMix = mixPct / 100.0;
        fPhaseInc = kTwoPi * rateHz / fSr;
    }

    double phase() const noexcept { return fPhase / kTwoPi; } // 0..1 of the cycle

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            if (fPhase >= kTwoPi)
                fPhase = fPhase - kTwoPi;
            const double lfo = std::sin(fPhase);
            fPhase = fPhase + fPhaseInc;

            const double mod = fDepth * 0.5;
            const double base = 5.0 + mod;
            const double ds = (base + lfo * mod) * fSr / 1000.0;
            double r = double(fW) - ds;
            r = r < 0.0 ? r + double(fSize) : r;
            const double ri = std::floor(r), rf = r - ri;
            const size_t i1 = size_t(ri), i2 = ri >= double(fSize - 1) ? 0 : size_t(ri) + 1;
            const double dl = fL[i1] + (fL[i2] - fL[i1]) * rf;
            const double dr = fR[i1] + (fR[i2] - fR[i1]) * rf;

            fL[size_t(fW)] = xl;
            fR[size_t(fW)] = xr;
            if (++fW >= fSize)
                fW = 0;

            outL[i] = static_cast<T>(xl * (1.0 - fMix) + dl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + dr * fMix);
        }
    }

private:
    double fSr = 48000.0, fPhase = 0.0, fPhaseInc = 0.0, fDepth = 3.0, fMix = 0.5;
    std::vector<double> fL, fR;
    int fSize = 1, fW = 0;
};

} // namespace nullchorus
