/*
 * NULL JSFX - Pumper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/pumper.jsfx (computed in double, like EEL2): a gain dip
 * on every beat. While the transport plays, the beat phase is the fractional part of the
 * host's beat position at the start of each block (the JSFX's @block); within the block, and
 * while the transport is stopped, the phase advances at the tempo.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullpumper {

// gain over the beat for phase ph (0..1) and depth (0..1), exactly as in @sample
inline double gainAt(double ph, double depth) noexcept
{
    double shape = ph < 0.02 ? 1.0 - ph / 0.02 : std::fmin(1.0, (ph - 0.02) / 0.6);
    shape = shape * shape * (3.0 - 2.0 * shape);
    return 1.0 - depth * 0.9 * (1.0 - shape);
}

class PumperProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; }
    void reset() noexcept { fPh = 0.0; }                       // @init
    void setAmount(double amount) noexcept { fDepth = amount / 100.0; } // @slider
    double phase() const noexcept { return fPh; }

    // playing: host transport running; beatPos: beat position at the block start; tempo in BPM
    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, bool playing, double beatPos,
                 double tempo) noexcept
    {
        const double sr = fSampleRate, depth = fDepth;
        double ph = fPh;
        if (playing)
            ph = beatPos - std::floor(beatPos);
        for (uint32_t i = 0; i < frames; ++i)
        {
            ph += tempo / 60.0 / sr;
            if (ph >= 1.0)
                ph -= 1.0;
            const double g = gainAt(ph, depth);
            const double l = inL[i], r = inR[i];
            outL[i] = static_cast<T>(l * g);
            outR[i] = static_cast<T>(r * g);
        }
        fPh = ph;
    }

private:
    double fSampleRate = 48000.0, fDepth = 0.5, fPh = 0.0;
};

} // namespace nullpumper
