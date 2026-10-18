/*
 * NULL JSFX - Auto Pan, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/auto_pan.jsfx, computed in double like EEL2: a sine LFO
 * moves the level between the left and right channels (Depth), blended with the dry signal
 * (Mix). With Tempo Sync the rate is a 1/2, 1/4, 1/8 or 1/16 note per cycle at the host tempo,
 * re-read every block as the JSFX's @block does.
 * Mode (appended in 2.4): SINE is the above; the step modes jump between pan positions, one step
 * per LFO cycle, gliding into each over the Smoothing share of it (raised cosine, at least 1 ms).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullautopan {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI

// cycles per beat for each Subdivision (1/2, 1/4, 1/8, 1/16)
inline double subdivisionMult(int subdivision) noexcept
{
    return subdivision == 0 ? 0.5 : subdivision == 1 ? 1.0 : subdivision == 2 ? 2.0 : subdivision == 3 ? 4.0 : 0.5;
}

// pan position of step k in step mode m (+1 = left, -1 = right, as the LFO): 1 = L R,
// 2 = L C R C, 3 = a stepped sweep L to R and back in 8, 4 = L C R, 5 = R C L
inline double stepValue(int m, double k) noexcept
{
    return m == 1 ? (k == 0.0 ? 1.0 : -1.0)
         : m == 2 ? (k == 0.0 ? 1.0 : k == 2.0 ? -1.0 : 0.0)
         : m == 3 ? (k <= 4.0 ? 1.0 - k * 0.5 : -1.0 + (k - 4.0) * 0.5)
         : m == 4 ? 1.0 - k
                  : k - 1.0;
}

// steps in the pattern of step mode m
inline int stepCount(int m) noexcept { return m == 1 ? 2 : m == 2 ? 4 : m == 3 ? 8 : 3; }

class AutoPanProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPhase = 0.0;
        fRate = 2.0;
        fPhaseInc = kTwoPi * fRate / fSr;
        fSync = false;
        fMult = 0.5;
        fStep = 0.0;
        fN = 2;
    }

    // @slider
    void setParams(double rateHz, double depthPct, double mixPct, bool sync, int subdivision, int mode = 0,
                   double smoothingPct = 20.0)
    {
        fMode = mode;
        fSmooth = smoothingPct / 100.0;
        fN = stepCount(mode);
        if (fStep >= fN)
            fStep = 0.0;
        fDepth = depthPct / 100.0;
        fMix = mixPct / 100.0;
        fSync = sync;
        if (subdivision >= 0 && subdivision <= 3)
            fMult = subdivisionMult(subdivision);
        if (!fSync)
        {
            fRate = rateHz;
            fPhaseInc = kTwoPi * fRate / fSr;
        }
    }

    double rate() const noexcept { return fRate; }            // LFO rate in Hz
    double phase() const noexcept { return fPhase / kTwoPi; } // 0..1 of the cycle
    int step() const noexcept { return int(fStep); }          // the step (step modes)
    int steps() const noexcept { return fN; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo)
    {
        if (fSync) // @block
        {
            fRate = (tempo / 60.0) * fMult;
            fPhaseInc = kTwoPi * fRate / fSr;
        }
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            if (fPhase >= kTwoPi)
            {
                fPhase = fPhase - kTwoPi;
                fStep += 1.0;
                if (fStep >= fN)
                    fStep = 0.0;
            }
            double lfo;
            if (fMode != 0)
            {
                // step mode: glide from the last step's position over the first Smoothing share
                const double t = fPhase / kTwoPi / std::max(fSmooth, 0.001 * fRate);
                const double c = stepValue(fMode, fStep);
                if (t < 1.0)
                {
                    const double p = stepValue(fMode, fStep > 0.0 ? fStep - 1.0 : double(fN - 1));
                    lfo = p + (c - p) * (0.5 - 0.5 * std::cos(3.141592653589793 * t));
                }
                else
                    lfo = c;
            }
            else
                lfo = std::sin(fPhase);
            fPhase = fPhase + fPhaseInc;
            const double lg = 0.5 + 0.5 * fDepth * lfo;
            const double rg = 0.5 - 0.5 * fDepth * lfo;
            const double pl = xl * lg, pr = xr * rg;
            outL[i] = static_cast<T>(xl * (1.0 - fMix) + pl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + pr * fMix);
        }
    }

private:
    double fSr = 48000.0, fPhase = 0.0, fRate = 2.0, fPhaseInc = 0.0, fMult = 0.5;
    double fDepth = 0.5, fMix = 0.4, fSmooth = 0.2, fStep = 0.0;
    int fMode = 0, fN = 2;
    bool fSync = false;
};

} // namespace nullautopan
