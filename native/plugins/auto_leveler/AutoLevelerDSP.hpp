/*
 * NULL JSFX - Auto-Leveler, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/auto_leveler.jsfx, computed in double like EEL2: the RMS
 * level (~300 ms) is compared with Target and the gain glides there at Speed, limited to Max
 * Boost / Max Cut; below Gate the gain holds.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nullautoleveler {

class AutoLevelerProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fMs = 0.0;
        fGainDb = 0.0;
    }

    // @slider
    void setParams(double target, double speedMs, double maxBoost, double maxCut, double gate)
    {
        fTarget = target;
        fRmsC = std::exp(-1.0 / (0.3 * fSr));
        fSpeedC = std::exp(-1.0 / (speedMs / 1000.0 * fSr));
        fMaxUp = maxBoost;
        fMaxDown = maxCut;
        fGate = gate;
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        fMs = fRmsC * fMs + (1.0 - fRmsC) * 0.5 * (l * l + r * r);
        const double levelDb = 10.0 * std::log10(std::max(fMs, 0.000000000001));
        if (levelDb > fGate)
        {
            const double want = std::max(-fMaxDown, std::min(fMaxUp, fTarget - levelDb));
            fGainDb = fSpeedC * fGainDb + (1.0 - fSpeedC) * want;
        }
        const double g = std::pow(10.0, fGainDb / 20.0);
        l *= g;
        r *= g;
    }

    double meanSquare() const noexcept { return fMs; }
    double gainDb() const noexcept { return fGainDb; }
    double rmsCoef() const noexcept { return fRmsC; }

private:
    double fSr = 48000.0, fMs = 0.0, fGainDb = 0.0;
    double fTarget = -18.0, fRmsC = 0.0, fSpeedC = 0.0, fMaxUp = 12.0, fMaxDown = 12.0, fGate = -50.0;
};

} // namespace nullautoleveler
