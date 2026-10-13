/*
 * NULL JSFX - Flanger, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/flanger.jsfx, computed in double like EEL2: a sine LFO sweeps
 * a stereo delay line between 0.5 ms and 0.5 ms + Depth (linear interpolation) with feedback
 * (up to 70%), blended with the dry signal (Mix).
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullflanger {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI

// the delay time in ms at LFO value lfo (-1..1)
inline double delayMsAt(double depthMs, double lfo) noexcept { return 0.5 + depthMs * 0.5 * (1.0 + lfo); }

class FlangerProcessor {
public:
    // @init: 50 ms buffers, cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 0.05));
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fPhase = 0.0;
        fFbL = fFbR = 0.0;
    }

    // @slider
    void setParams(double rateHz, double depthMs, double feedbackPct, double mixPct)
    {
        fDepth = depthMs;
        fFb = feedbackPct / 100.0 * 0.7;
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

            const double ds = (0.5 + fDepth * 0.5 * (1.0 + lfo)) * fSr / 1000.0;
            double r = double(fW) - ds;
            r = r < 0.0 ? r + double(fSize) : r;
            const double ri = std::floor(r), rf = r - ri;
            const size_t i1 = size_t(ri), i2 = ri >= double(fSize - 1) ? 0 : size_t(ri) + 1;
            const double dl = fL[i1] + (fL[i2] - fL[i1]) * rf;
            const double dr = fR[i1] + (fR[i2] - fR[i1]) * rf;

            fL[size_t(fW)] = xl + fFbL * fFb;
            fR[size_t(fW)] = xr + fFbR * fFb;
            fFbL = dl;
            fFbR = dr;
            if (++fW >= fSize)
                fW = 0;

            outL[i] = static_cast<T>(xl * (1.0 - fMix) + dl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + dr * fMix);
        }
    }

private:
    double fSr = 48000.0, fPhase = 0.0, fPhaseInc = 0.0, fDepth = 5.0, fFb = 0.35, fMix = 0.4;
    double fFbL = 0.0, fFbR = 0.0;
    std::vector<double> fL, fR;
    int fSize = 1, fW = 0;
};

} // namespace nullflanger
