/*
 * NULL JSFX - Snappy, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/snappy.jsfx, computed in double like EEL2:
 *
 *   lev   = max(|l|, |r|)
 *   fast  follows lev with 1 ms attack, slow with 25 ms attack, both 80 ms release
 *   trans = fast > 1e-5 ? clamp((fast - slow)/fast, 0, 1) : 0
 *   g     = 10^((9a*trans - 3a*(1 - trans))/20),  a = Amount/100;  y = x*g
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullsnappy {

class SnappyProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fFa = std::exp(-1.0 / (0.001 * sr));
        fSa = std::exp(-1.0 / (0.025 * sr));
        fRe = std::exp(-1.0 / (0.08 * sr));
    }
    void setAmount(double slider) noexcept
    {
        const double amount = slider / 100.0;
        fBoost = amount * 9.0;
        fCut = amount * 3.0;
    }
    void reset() noexcept { fFast = fSlow = 0.0; }

    // applies the gain to l/r and returns it
    inline double processSample(double& l, double& r) noexcept
    {
        const double lev = std::max(std::fabs(l), std::fabs(r));
        fFast = lev > fFast ? fFa * fFast + (1.0 - fFa) * lev : fRe * fFast + (1.0 - fRe) * lev;
        fSlow = lev > fSlow ? fSa * fSlow + (1.0 - fSa) * lev : fRe * fSlow + (1.0 - fRe) * lev;
        const double trans = fFast > 0.00001 ? std::max(0.0, std::min(1.0, (fFast - fSlow) / fFast)) : 0.0;
        const double g = std::pow(10.0, (fBoost * trans - fCut * (1.0 - trans)) / 20.0);
        l *= g;
        r *= g;
        return g;
    }

private:
    double fFa = 0.0, fSa = 0.0, fRe = 0.0;
    double fBoost = 4.5, fCut = 1.5;
    double fFast = 0.0, fSlow = 0.0;
};

} // namespace nullsnappy
