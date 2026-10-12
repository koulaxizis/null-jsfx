/*
 * NULL JSFX - Transient Shaper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/transient_shaper.jsfx, computed in double like EEL2: a fast
 * (1 / 20 ms) and a slow (20 / 150 ms) envelope of the stereo-linked peak; where the fast one
 * runs ahead (relative to the slow one, 0..1) the Attack gain applies, elsewhere the Sustain
 * gain, blended with the dry signal by Mix.
 */

#pragma once

#include <cmath>

namespace nulltransient {

class TransientProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fFast = fSlow = 0.0;
        fTrans = 0.0;
        fTotal = 1.0;
    }

    // @slider
    void setParams(double attackDb, double sustainDb, double mixPct)
    {
        fMix = mixPct / 100.0;
        fAttackBoost = std::exp(attackDb * 0.11512925464970228420);
        fSustainBoost = std::exp(sustainDb * 0.11512925464970228420);
        fFa = 1.0 - std::exp(-1.0 / (0.001 * fSr));
        fFr = 1.0 - std::exp(-1.0 / (0.020 * fSr));
        fSa = 1.0 - std::exp(-1.0 / (0.020 * fSr));
        fSrc = 1.0 - std::exp(-1.0 / (0.150 * fSr));
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        const double al = l < 0.0 ? -l : l, ar = r < 0.0 ? -r : r;
        const double in = al > ar ? al : ar;
        fIn = in;
        if (in > fFast)
            fFast = fFast + fFa * (in - fFast);
        else
            fFast = fFast + fFr * (in - fFast);
        if (in > fSlow)
            fSlow = fSlow + fSa * (in - fSlow);
        else
            fSlow = fSlow + fSrc * (in - fSlow);
        double t = fSlow > 0.0001 ? (fFast - fSlow) / fSlow : 0.0;
        t = t > 1.0 ? 1.0 : t;
        t = t < 0.0 ? 0.0 : t;
        fTrans = t;
        const double s = 1.0 - t;
        fTotal = 1.0 + (fAttackBoost - 1.0) * t + (fSustainBoost - 1.0) * s;
        const double ol = l * fTotal, orr = r * fTotal;
        l = l * (1.0 - fMix) + ol * fMix;
        r = r * (1.0 - fMix) + orr * fMix;
    }

    double input() const noexcept { return fIn; }
    double transient() const noexcept { return fTrans; }
    // the gain after Mix (for the display)
    double gain() const noexcept { return 1.0 - fMix + fTotal * fMix; }

private:
    double fSr = 48000.0, fFast = 0.0, fSlow = 0.0, fTrans = 0.0, fTotal = 1.0, fIn = 0.0;
    double fMix = 1.0, fAttackBoost = 1.0, fSustainBoost = 1.0, fFa = 1.0, fFr = 1.0, fSa = 1.0, fSrc = 1.0;
};

} // namespace nulltransient
