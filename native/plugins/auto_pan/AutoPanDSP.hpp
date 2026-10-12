/*
 * NULL JSFX - Auto Pan, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/auto_pan.jsfx, computed in double like EEL2: a sine LFO
 * moves the level between the left and right channels (Depth), blended with the dry signal
 * (Mix). With Tempo Sync the rate is a 1/2, 1/4, 1/8 or 1/16 note per cycle at the host tempo,
 * re-read every block as the JSFX's @block does.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullautopan {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI

// cycles per beat for each Subdivision (1/2, 1/4, 1/8, 1/16)
inline double subdivisionMult(int subdivision) noexcept
{
    return subdivision == 0 ? 0.5 : subdivision == 1 ? 1.0 : subdivision == 2 ? 2.0 : subdivision == 3 ? 4.0 : 0.5;
}

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
    }

    // @slider
    void setParams(double rateHz, double depthPct, double mixPct, bool sync, int subdivision)
    {
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
                fPhase = fPhase - kTwoPi;
            const double lfo = std::sin(fPhase);
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
    double fDepth = 0.5, fMix = 0.4;
    bool fSync = false;
};

} // namespace nullautopan
