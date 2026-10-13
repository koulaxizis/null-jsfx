/*
 * NULL JSFX - Gate, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/gate.jsfx, computed in double like EEL2: a stereo-linked peak
 * detector (4.5 ms fall) on an optional high-passed sidechain opens the gate above Threshold
 * (Attack) and closes it below (Release) down to Range (-80 dB = fully closed).
 */

#pragma once

#include <cmath>

namespace nullgate {

class GateProcessor {
public:
    // @init (with the JSFX's hard-coded defaults, which @slider then overrides)
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPkDecay = std::exp(-1.0 / (0.0045 * sr));
        fScHpf = 0.0;
        fThresholdLin = std::pow(10.0, -40.0 / 20.0);
        coefs(10.0, 200.0);
        fGateLevel = 0.0;
        fEnvL = fEnvR = 0.0;
        fLpfL = fLpfR = 0.0;
        fLpfCoef = 0.0;
        fEnv = 0.0;
        fGain = 0.0;
    }

    // @slider
    void setParams(double thresholdDb, double attackMs, double releaseMs, double scHpf, double rangeDb)
    {
        fScHpf = scHpf;
        fRangeLin = rangeDb <= -80.0 ? 0.0 : std::pow(10.0, rangeDb / 20.0);
        fThresholdLin = std::pow(10.0, thresholdDb / 20.0);
        coefs(attackMs, releaseMs);
        if (fScHpf > 0.0)
            fLpfCoef = 1.0 - std::exp(-2.0 * 3.14159265358979 * fScHpf / fSr);
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        double il, ir;
        if (fScHpf > 0.0)
        {
            fLpfL = fLpfL + fLpfCoef * (l - fLpfL);
            fLpfR = fLpfR + fLpfCoef * (r - fLpfR);
            il = l - fLpfL;
            ir = r - fLpfR;
        }
        else
        {
            il = l;
            ir = r;
        }
        const double al = il < 0.0 ? -il : il;
        const double ar = ir < 0.0 ? -ir : ir;
        const double dl = fEnvL * fPkDecay, dr = fEnvR * fPkDecay;
        fEnvL = al > dl ? al : dl;
        fEnvR = ar > dr ? ar : dr;
        fEnv = fEnvL > fEnvR ? fEnvL : fEnvR;
        if (fEnv > fThresholdLin)
        {
            if (fGateLevel < 1.0)
                fGateLevel = fGateLevel + fAttackCoef * (1.0 - fGateLevel);
        }
        else if (fGateLevel > 0.0)
            fGateLevel = fGateLevel + fReleaseCoef * (0.0 - fGateLevel);
        fGain = fRangeLin + (1.0 - fRangeLin) * fGateLevel;
        l = l * fGain;
        r = r * fGain;
    }

    double detector() const noexcept { return fEnv; }
    double gain() const noexcept { return fGain; }

private:
    void coefs(double attackMs, double releaseMs)
    {
        const double at = fSr * attackMs / 1000.0, rt = fSr * releaseMs / 1000.0;
        fAttackCoef = 1.0 - std::exp(-1.0 / (at > 1.0 ? at : 1.0));
        fReleaseCoef = 1.0 - std::exp(-1.0 / (rt > 1.0 ? rt : 1.0));
    }

    double fSr = 48000.0, fPkDecay = 0.0, fScHpf = 0.0, fRangeLin = 0.0, fThresholdLin = 0.01;
    double fAttackCoef = 0.0, fReleaseCoef = 0.0, fLpfCoef = 0.0;
    double fGateLevel = 0.0, fEnvL = 0.0, fEnvR = 0.0, fLpfL = 0.0, fLpfR = 0.0, fEnv = 0.0, fGain = 0.0;
};

} // namespace nullgate
