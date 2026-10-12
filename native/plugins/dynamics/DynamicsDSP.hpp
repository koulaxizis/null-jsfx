/*
 * NULL JSFX - Dynamics, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/dynamics.jsfx, computed in double like EEL2: a stereo-linked
 * peak compressor (above Threshold) or downward expander (below it) with Ratio, Knee, Attack,
 * Release and Makeup. ln10 is the JSFX's truncated 2.302585, kept for an exact match.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nulldynamics {

static constexpr double kLn10 = 2.302585;
static constexpr double kDbToLin = kLn10 / 20.0;

struct Params {
    bool expander = false;
    double thresholdDb = -20.0, ratio = 4.0, kneeDb = 6.0, makeupDb = 0.0;
};

// gain reduction in dB for an input level in dB (the JSFX's gain computer)
inline double grDb(const Params& p, double inputDb) noexcept
{
    const double x = p.expander ? p.thresholdDb - inputDb : inputDb - p.thresholdDb;
    if (x <= 0.0)
        return 0.0;
    if (p.kneeDb > 0.0 && x < p.kneeDb)
        return x * (1.0 - 1.0 / p.ratio) * (x / p.kneeDb);
    return x * (1.0 - 1.0 / p.ratio);
}

// static curve: output level in dB for an input level d (for the display)
inline double curveDb(const Params& p, double d) noexcept { return d - grDb(p, d) + p.makeupDb; }

class DynamicsProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fGainEnv = 1.0;
        fInputAbs = 0.0;
    }

    // @slider
    void setParams(int mode, double thresholdDb, double ratio, double kneeDb, double attackMs, double releaseMs, double makeupDb)
    {
        fP.expander = std::fabs(double(mode)) >= 0.00001; // EEL2: mode == 0 is fuzzy
        fP.thresholdDb = thresholdDb;
        fP.ratio = ratio;
        fP.kneeDb = kneeDb;
        fP.makeupDb = makeupDb;
        fAttack = attackMs > 0.0 ? 1.0 - std::exp(-1.0 / (attackMs * fSr / 1000.0)) : 1.0;
        fRelease = releaseMs > 0.0 ? 1.0 - std::exp(-1.0 / (releaseMs * fSr / 1000.0)) : 1.0;
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        double in = std::max(std::fabs(l), std::fabs(r));
        if (in < 0.000001)
            in = 0.000001;
        fInputAbs = in;
        const double inputDb = std::log(in) / kLn10 * 20.0;
        const double target = std::exp(-grDb(fP, inputDb) * kDbToLin);
        if (target < fGainEnv)
            fGainEnv = fGainEnv + (target - fGainEnv) * fAttack;
        else
            fGainEnv = fGainEnv + (target - fGainEnv) * fRelease;
        const double makeup = std::exp(fP.makeupDb * kDbToLin);
        l = l * fGainEnv * makeup;
        r = r * fGainEnv * makeup;
    }

    double gain() const noexcept { return fGainEnv; }
    double inputAbs() const noexcept { return fInputAbs; }

private:
    Params fP;
    double fSr = 48000.0, fAttack = 1.0, fRelease = 1.0, fGainEnv = 1.0, fInputAbs = 0.0;
};

} // namespace nulldynamics
