/*
 * NULL JSFX - Envelope Follower, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/envelope_follower.jsfx, computed in double like EEL2: the
 * stereo-linked peak (times Sensitivity) drives an envelope (Attack / Release) whose -48..0 dB
 * span maps to 0..1 (inverted with Direction Down); that position moves a TPT state-variable
 * filter (low-, band- or high-pass) from Low Freq to High Freq, or sets the volume, then Mix.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nullenvfollower {

class EnvelopeFollowerProcessor {
public:
    // @init (the JSFX's unset variables start at 0)
    void setSampleRate(double sr)
    {
        fSr = sr;
        fEnv = 0.0;
        fE = 0.0;
        fFc = 0.0;
        fA1 = fA2 = fA3 = 0.0;
        fL1 = fL2 = fR1 = fR2 = 0.0;
    }

    // @slider
    void setParams(int target, double attackMs, double releaseMs, double sensDb, double lowHz, double highHz,
                   double resonance, bool down, double mixPct)
    {
        fTarget = target;
        fAtt = std::exp(-1.0 / (attackMs / 1000.0 * fSr));
        fRel = std::exp(-1.0 / (releaseMs / 1000.0 * fSr));
        fSens = std::pow(10.0, sensDb / 20.0);
        fLo = lowHz;
        fHi = std::max(highHz, lowHz);
        fK = 1.0 / resonance;
        fDown = down;
        fMix = mixPct / 100.0;
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        const double lev = std::max(std::fabs(l), std::fabs(r)) * fSens;
        fEnv = lev > fEnv ? fAtt * fEnv + (1.0 - fAtt) * lev : fRel * fEnv + (1.0 - fRel) * lev;
        double e = fEnv > 0.004 ? std::min(1.0, 1.0 + std::log10(fEnv) * 20.0 / 48.0) : 0.0;
        e = std::max(0.0, e);
        if (fDown)
            e = 1.0 - e;
        fE = e;

        double wl, wr;
        if (fTarget == 3)
        {
            wl = l * e;
            wr = r * e;
        }
        else
        {
            fFc = std::min(fLo * std::pow(fHi / fLo, e), fSr * 0.45);
            const double g = std::tan(3.141592653589793 * fFc / fSr);
            fA1 = 1.0 / (1.0 + g * (g + fK));
            fA2 = g * fA1;
            fA3 = g * fA2;
            wl = svf(l, fL1, fL2);
            wr = svf(r, fR1, fR2);
        }
        l = l * (1.0 - fMix) + wl * fMix;
        r = r * (1.0 - fMix) + wr * fMix;
    }

    double position() const noexcept { return fE; }  // envelope position 0..1 (after Direction)
    double cutoff() const noexcept { return fFc; }    // filter cutoff in Hz (last filter sample)

private:
    double svf(double x, double& ic1, double& ic2) const noexcept
    {
        const double v3 = x - ic2;
        const double v1 = fA1 * ic1 + fA2 * v3;
        const double v2 = ic2 + fA2 * ic1 + fA3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        const double hp = x - fK * v1 - v2;
        return fTarget == 0 ? v2 : fTarget == 1 ? v1 : hp;
    }

    double fSr = 48000.0, fEnv = 0.0, fE = 0.0, fFc = 0.0;
    double fAtt = 0.0, fRel = 0.0, fSens = 1.0, fLo = 200.0, fHi = 3000.0, fK = 1.0 / 3.0, fMix = 1.0;
    double fA1 = 0.0, fA2 = 0.0, fA3 = 0.0, fL1 = 0.0, fL2 = 0.0, fR1 = 0.0, fR2 = 0.0;
    int fTarget = 0;
    bool fDown = false;
};

} // namespace nullenvfollower
