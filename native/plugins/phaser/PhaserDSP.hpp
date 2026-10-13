/*
 * NULL JSFX - Phaser, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/phaser.jsfx, computed in double like EEL2: 1 to 8 first-order
 * allpass stages per channel whose frequency a sine LFO sweeps between 440 Hz and 1.6 kHz,
 * with 30% feedback; the allpass output is added to the dry signal (Mix).
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullphaser {

static constexpr double kTwoPi = 2.0 * M_PI; // 2 * $pi

// the allpass frequency in Hz at LFO value lfo (-1..1): d * srate / 2 of the JSFX
inline double sweepHzAt(double lfo) noexcept { return 440.0 + 580.0 * (lfo + 1.0); }

class PhaserProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPhase = 0.0;
        for (int k = 0; k < 8; ++k)
            fLy[k] = fLx[k] = fRy[k] = fRx[k] = 0.0;
        fLastL = fLastR = 0.0;
    }

    // @slider
    void setParams(double rateHz, double stages, double mixPct)
    {
        fStages = int(std::floor(stages));
        fMix = mixPct / 100.0;
        fInc = 2.0 * M_PI * rateHz / fSr;
        double dMin = 440.0 * 2.0 / fSr;
        const double dMax = 1600.0 * 2.0 / fSr;
        fDSc = std::fabs(dMax - dMin) * 0.5;
        dMin = dMin < dMax ? dMin : dMax;
        fDMin = dMin;
    }

    double phase() const noexcept { return fPhase / kTwoPi; } // 0..1 of the cycle

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            fPhase += fInc;
            if (fPhase >= 2.0 * M_PI)
                fPhase -= 2.0 * M_PI;
            const double d = fDMin + fDSc * (std::sin(fPhase) + 1.0);
            const double coef = (1.0 - d) / (1.0 + d);

            const double xl = inL[i];
            double wl = xl + fLastL * kFeedback;
            for (int k = 0; k < fStages && k < 8; ++k)
            {
                fLy[k] = coef * (fLy[k] + wl) - fLx[k];
                fLx[k] = wl;
                wl = fLy[k];
            }
            fLastL = wl;
            outL[i] = static_cast<T>(xl + wl * fMix);

            const double xr = inR[i];
            double wr = xr + fLastR * kFeedback;
            for (int k = 0; k < fStages && k < 8; ++k)
            {
                fRy[k] = coef * (fRy[k] + wr) - fRx[k];
                fRx[k] = wr;
                wr = fRy[k];
            }
            fLastR = wr;
            outR[i] = static_cast<T>(xr + wr * fMix);
        }
    }

private:
    static constexpr double kFeedback = 0.3;
    double fSr = 48000.0, fPhase = 0.0, fInc = 0.0, fMix = 0.3, fDMin = 0.0, fDSc = 0.0;
    double fLy[8] = {}, fLx[8] = {}, fRy[8] = {}, fRx[8] = {};
    double fLastL = 0.0, fLastR = 0.0;
    int fStages = 4;
};

} // namespace nullphaser
