/*
 * NULL JSFX - Tremolo, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/tremolo.jsfx, computed in double like EEL2: a sine LFO
 * moves the gain between 1 (LFO peak) and 1 - Depth (trough), blended with the dry signal (Mix).
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulltremolo {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI

// the output level (dry and wet together) at LFO value lfo (-1..1)
inline double levelAt(double depthPct, double mixPct, double lfo) noexcept
{
    return 1.0 - mixPct / 100.0 * depthPct / 100.0 * (0.5 - 0.5 * lfo);
}

class TremoloProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPhase = 0.0;
    }

    // @slider
    void setParams(double rateHz, double depthPct, double mixPct)
    {
        fDepth = depthPct / 100.0;
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
            const double a = 1.0 - fDepth * (0.5 - 0.5 * lfo);
            const double pl = xl * a, pr = xr * a;
            outL[i] = static_cast<T>(xl * (1.0 - fMix) + pl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + pr * fMix);
        }
    }

private:
    double fSr = 48000.0, fPhase = 0.0, fPhaseInc = 0.0, fDepth = 0.5, fMix = 0.4;
};

} // namespace nulltremolo
