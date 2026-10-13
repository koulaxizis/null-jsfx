/*
 * NULL JSFX - Breath Control, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/breath_control.jsfx, computed in double like EEL2: a
 * stereo-linked power detector (1 ms rise, 30 ms fall) with 3 dB hysteresis around Threshold;
 * below it the gain moves to Range (Release), above it back to unity (Attack).
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nullbreath {

class BreathProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fEnv = 0.0;
        fGain = 1.0;
        fOpen = true;
    }

    // @slider
    void setParams(double thresholdDb, double attackMs, double releaseMs, double rangeDb)
    {
        const double thr = std::pow(10.0, thresholdDb / 20.0);
        fThrOpen = thr * thr;
        fThrClose = fThrOpen * 0.5;
        fAtt = 1.0 - std::exp(-1.0 / (std::max(attackMs, 0.1) * fSr / 1000.0));
        fRel = 1.0 - std::exp(-1.0 / (std::max(releaseMs, 1.0) * fSr / 1000.0));
        fRange = std::pow(10.0, rangeDb / 20.0);
        fEnvUp = 1.0 - std::exp(-1.0 / (0.001 * fSr));
        fEnvDn = 1.0 - std::exp(-1.0 / (0.03 * fSr));
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        const double pw = std::max(l * l, r * r);
        fEnv += (pw > fEnv ? fEnvUp : fEnvDn) * (pw - fEnv);
        fOpen = fOpen ? (fEnv > fThrClose) : (fEnv > fThrOpen);
        const double target = fOpen ? 1.0 : fRange;
        fGain += (target > fGain ? fAtt : fRel) * (target - fGain);
        l *= fGain;
        r *= fGain;
    }

    double envelope() const noexcept { return fEnv; } // detector power
    double gain() const noexcept { return fGain; }

private:
    double fSr = 48000.0, fThrOpen = 0.0, fThrClose = 0.0, fAtt = 1.0, fRel = 1.0, fRange = 1.0;
    double fEnvUp = 1.0, fEnvDn = 1.0, fEnv = 0.0, fGain = 1.0;
    bool fOpen = true;
};

} // namespace nullbreath
