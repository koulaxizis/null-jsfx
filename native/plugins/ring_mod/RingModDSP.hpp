/*
 * NULL JSFX - Ring Mod, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/ring_mod.jsfx, computed in double like EEL2: the input is
 * multiplied by a sine carrier whose frequency an LFO at Rate sweeps up and down by up to one
 * octave (Depth), blended with the dry signal (Mix).
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullringmod {

static constexpr double kTwoPi = 2.0 * M_PI; // 2 * $pi

// the carrier frequency in Hz at LFO value lfo (-1..1)
inline double carrierAt(double carrierHz, double depthPct, double lfo) noexcept
{
    return carrierHz * std::pow(2.0, depthPct / 100.0 * lfo);
}

class RingModProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fCPhase = 0.0;
        fLPhase = 0.0;
    }

    // @slider
    void setParams(double rateHz, double carrierHz, double depthPct, double mixPct)
    {
        fCarrier = carrierHz;
        fLInc = 2.0 * M_PI * rateHz / fSr;
        fDepth = depthPct / 100.0;
        fMix = mixPct / 100.0;
    }

    double phase() const noexcept { return fLPhase / kTwoPi; } // LFO, 0..1 of the cycle

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            fLPhase += fLInc;
            if (fLPhase >= 2.0 * M_PI)
                fLPhase -= 2.0 * M_PI;
            const double freq = fCarrier * std::pow(2.0, fDepth * std::sin(fLPhase));
            fCPhase += 2.0 * M_PI * freq / fSr;
            if (fCPhase >= 2.0 * M_PI)
                fCPhase -= 2.0 * M_PI;
            const double c = std::sin(fCPhase);
            const double xl = inL[i], xr = inR[i];
            outL[i] = static_cast<T>(xl * (1.0 - fMix) + xl * c * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + xr * c * fMix);
        }
    }

private:
    double fSr = 48000.0, fCPhase = 0.0, fLPhase = 0.0, fLInc = 0.0;
    double fCarrier = 200.0, fDepth = 0.5, fMix = 0.4;
};

} // namespace nullringmod
