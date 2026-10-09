/*
 * NULL JSFX - Grit, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/grit.jsfx, computed in double like EEL2:
 *
 *   g     = Intensity/100,  steps = 2^(16 - 13g - 1),  hop = 1/(1 + 7g)
 *   acc  += hop; acc >= 1 -> acc -= 1, hold the input      (sample-rate reduction)
 *   q     = floor(held*steps + 0.5)/steps                   (bit reduction)
 *   y     = x + g*(q - x)
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullgrit {

struct Params {
    double g, steps, hop;
};

// the JSFX @slider section
inline Params paramsFor(double slider) noexcept
{
    Params p;
    p.g = slider / 100.0;
    p.steps = std::pow(2.0, 16.0 - p.g * 13.0 - 1.0);
    p.hop = 1.0 / (1.0 + p.g * 7.0);
    return p;
}

// static transfer curve (the sample hold left out), for the display
inline double curve(const Params& p, double x) noexcept
{
    return x + p.g * (std::floor(x * p.steps + 0.5) / p.steps - x);
}

class GritProcessor {
public:
    void setAmount(double slider) noexcept { fP = paramsFor(slider); }
    void reset() noexcept { fAcc = 1.0; fHeldL = fHeldR = 0.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Params p = fP;
        for (uint32_t i = 0; i < frames; ++i) {
            double l = inL[i], r = inR[i];
            fAcc += p.hop;
            if (fAcc >= 1.0) {
                fAcc -= 1.0;
                fHeldL = l;
                fHeldR = r;
            }
            const double cl = std::floor(fHeldL * p.steps + 0.5) / p.steps;
            const double cr = std::floor(fHeldR * p.steps + 0.5) / p.steps;
            l += p.g * (cl - l);
            r += p.g * (cr - r);
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    Params fP = paramsFor(0.0);
    double fAcc = 1.0, fHeldL = 0.0, fHeldR = 0.0;
};

} // namespace nullgrit
