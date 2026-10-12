/*
 * NULL JSFX - Sidechain Ducker, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/sidechain_ducker.jsfx, computed in double like EEL2: the key
 * (the sidechain pair, inputs 3/4, or the main input), optionally high-passed, drives a peak
 * detector (instant attack, 10 ms release); above Threshold, and for Hold after it, the gain
 * moves towards Depth with Attack, otherwise back to unity with Release. Stereo-linked.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullducker {

class DuckerProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fEnv = 0.0;
        fGain = 1.0;
        fHeld = 0.0; // (the key high-pass keeps its state, as in the JSFX)
    }

    // @slider
    void setParams(double thrDb, double depthDb, double attackMs, double holdMs, double releaseMs, bool mainKey,
                   double hpHz)
    {
        fThr = std::pow(10.0, thrDb / 20.0);
        fFloor = std::pow(10.0, depthDb / 20.0);
        fAtt = std::exp(-1.0 / (attackMs * 0.001 * fSr));
        fRel = std::exp(-1.0 / (releaseMs * 0.001 * fSr));
        fHoldN = std::floor(holdMs * 0.001 * fSr);
        fDetRel = std::exp(-1.0 / (0.01 * fSr));
        fUseHp = hpHz > 0.0;
        fHpL.c = fHpR.c = fUseHp ? std::exp(-2.0 * M_PI * hpHz / fSr) : 1.0;
        fMainKey = mainKey;
    }

    double env() const noexcept { return fEnv; }
    double gain() const noexcept { return fGain; }

    template <typename T>
    void process(const T* inL, const T* inR, const T* scL, const T* scR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            double kl = fMainKey ? xl : double(scL[i]);
            double kr = fMainKey ? xr : double(scR[i]);
            if (fUseHp)
            {
                kl = fHpL.run(kl);
                kr = fHpR.run(kr);
            }
            const double lev = std::max(std::fabs(kl), std::fabs(kr));
            fEnv = lev > fEnv ? lev : fDetRel * fEnv + (1.0 - fDetRel) * lev;
            if (fEnv > fThr)
                fHeld = fHoldN;
            else if (fHeld > 0.0)
                fHeld -= 1.0;
            const double target = fEnv > fThr || fHeld > 0.0 ? fFloor : 1.0;
            fGain = target < fGain ? fAtt * fGain + (1.0 - fAtt) * target : fRel * fGain + (1.0 - fRel) * target;
            outL[i] = static_cast<T>(xl * fGain);
            outR[i] = static_cast<T>(xr * fGain);
        }
    }

private:
    // one-pole high-pass (the JSFX's hp)
    struct HighPass {
        double c = 1.0, y = 0.0, x1 = 0.0;
        double run(double x) noexcept
        {
            y = c * (y + x - x1);
            x1 = x;
            return y;
        }
    };

    double fSr = 48000.0;
    double fEnv = 0.0, fGain = 1.0, fHeld = 0.0, fHoldN = 0.0;
    double fThr = 0.0, fFloor = 1.0, fAtt = 0.0, fRel = 0.0, fDetRel = 0.0;
    bool fUseHp = false, fMainKey = false;
    HighPass fHpL, fHpR;
};

// the ducking shape for the display (as sd_step in the GFX): the gain in dB after moving dt ms
// at time t ms of one key hit (key above Threshold for 5 attack times, then Hold, then free)
struct DuckShape {
    double g = 1.0;
    double step(double t, double dt, double depthDb, double attackMs, double holdMs, double releaseMs) noexcept
    {
        const double tg = t < 5.0 * attackMs + holdMs ? std::pow(10.0, depthDb / 20.0) : 1.0;
        const double c = std::exp(-dt / (tg < g ? attackMs : releaseMs));
        g = c * g + (1.0 - c) * tg;
        return 20.0 * std::log10(std::max(g, 0.00001));
    }
};

} // namespace nullducker
