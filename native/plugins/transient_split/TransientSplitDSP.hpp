/*
 * NULL JSFX - Transient Split, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/transient_split.jsfx, computed in double in the EEL2 order: a
 * fast (1 ms) and a slow (25 ms) peak envelope of the louder channel, both with a 100 ms release;
 * the transient weight w = (fast - slow) / fast x Sensitivity jumps up at once and falls back
 * over Decay; transient = x w, tonal = x - x w. Balance: both, each with its level, on 1/2;
 * Split: transient on 1/2, tonal on 3/4.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulltransientsplit {

struct Params {
    int mode = 0;
    double transient = 0.0, tonal = 0.0, sensitivity = 50.0, decay = 60.0, output = 0.0;
};

struct Coeffs {
    bool split = false;
    double gt = 1.0, gn = 1.0, sens = 2.25, af = 1.0, as = 1.0, ar = 1.0, cd = 1.0, og = 1.0;
};

// the @slider code of transient_split.jsfx
inline Coeffs coeffsFor(const Params& p, double sr) noexcept
{
    Coeffs c;
    c.split = p.mode != 0;
    c.gt = std::pow(10.0, p.transient / 20.0);
    c.gn = std::pow(10.0, p.tonal / 20.0);
    c.sens = 0.5 + p.sensitivity / 100.0 * 3.5;
    c.af = 1.0 - std::exp(-1.0 / (0.001 * sr));
    c.as = 1.0 - std::exp(-1.0 / (0.025 * sr));
    c.ar = 1.0 - std::exp(-1.0 / (0.1 * sr));
    c.cd = 1.0 - std::exp(-1.0 / (p.decay * 0.001 * sr));
    c.og = std::pow(10.0, p.output / 20.0);
    return c;
}

class TransientSplitProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        fC = coeffsFor(fP, fSr);
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fC = coeffsFor(fP, fSr);
    }

    void reset() noexcept { fEf = fEs = fW = 0.0; }

    // the transient weight, for the display
    double weight() const noexcept { return fW; }

    // in: l, r; out: o[0..3] (channels 1-4)
    void processSample(double l, double r, double* o) noexcept
    {
        const Coeffs& c = fC;
        const double k = std::fmax(std::fabs(l), std::fabs(r));
        fEf += (k - fEf) * (k > fEf ? c.af : c.ar);
        fEs += (k - fEs) * (k > fEs ? c.as : c.ar);
        const double tw = fEf > 0.00001 ? std::fmin(1.0, std::fmax(0.0, (fEf - fEs) / fEf * c.sens)) : 0.0;
        if (tw > fW)
            fW = tw;
        else
            fW += (tw - fW) * c.cd;
        if (fW < 0.000000001)
            fW = 0.0;
        const double tl = l * fW, tr = r * fW;
        const double nl = l - tl, nr = r - tr;
        if (c.split)
        {
            o[0] = tl * c.gt * c.og;
            o[1] = tr * c.gt * c.og;
            o[2] = nl * c.gn * c.og;
            o[3] = nr * c.gn * c.og;
        }
        else
        {
            o[0] = (tl * c.gt + nl * c.gn) * c.og;
            o[1] = (tr * c.gt + nr * c.gn) * c.og;
            o[2] = 0.0;
            o[3] = 0.0;
        }
    }

private:
    double fSr = 48000.0;
    Params fP;
    Coeffs fC = coeffsFor(Params(), 48000.0);
    double fEf = 0.0, fEs = 0.0, fW = 0.0;
};

} // namespace nulltransientsplit
