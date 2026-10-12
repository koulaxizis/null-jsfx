/*
 * NULL JSFX - Noise Generator, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/noise_generator.jsfx, computed in double like EEL2: white,
 * pink (Paul Kellet's filter) or brown (leaky integrator) noise at a level in dB, with a
 * stereo width from mono (0%) to two independent channels (100%). The input is replaced.
 * The noise source is the JSFX's Park-Miller generator in exact double arithmetic, so the
 * output matches the JSFX sample for sample.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullnoise {

class NoiseProcessor {
public:
    // @init
    void setSampleRate(double)
    {
        for (int i = 0; i < 7; ++i)
            fPl[i] = fPr[i] = 0.0;
        fBl = fBr = 0.0;
        fSeed = 12345.0;
    }

    // @slider
    void setParams(int type, double levelDb, double widthPct)
    {
        fType = type;
        fWidth = widthPct / 100.0;
        fGain = std::exp(levelDb * 0.11512925464970228420);
    }

    template <typename T>
    void process(T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double rl = next();
            double rr = next();
            rr = rl * (1.0 - fWidth) + rr * fWidth;
            double nl = fNl, nr = fNr;
            if (fType == 0)
            {
                nl = rl * fGain;
                nr = rr * fGain;
            }
            if (fType == 1)
            {
                nl = pink(fPl, rl) * 0.11 * fGain;
                nr = pink(fPr, rr) * 0.11 * fGain;
            }
            if (fType == 2)
            {
                nl = brown(fBl, rl) * fGain * 3.5;
                nr = brown(fBr, rr) * fGain * 3.5;
            }
            fNl = nl;
            fNr = nr;
            outL[i] = static_cast<T>(nl);
            outR[i] = static_cast<T>(nr);
        }
    }

private:
    double next() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 1073741824.0 - 1.0;
    }

    static double pink(double* b, double r) noexcept
    {
        b[0] = 0.99886 * b[0] + r * 0.0555179;
        b[1] = 0.99332 * b[1] + r * 0.0750759;
        b[2] = 0.96900 * b[2] + r * 0.1538520;
        b[3] = 0.86600 * b[3] + r * 0.3104856;
        b[4] = 0.55000 * b[4] + r * 0.5329522;
        b[5] = 0.31300 * b[5] + r * 0.5329522;
        b[6] = 0.11000 * b[6] + r * 0.5329522;
        return b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + r * 0.5362;
    }

    static double brown(double& b, double r) noexcept
    {
        b = b + r * 0.02;
        b = b * 0.997;
        if (b > 1.0)
            b = 1.0;
        else if (b < -1.0)
            b = -1.0;
        return b;
    }

    double fPl[7] = {}, fPr[7] = {}, fBl = 0.0, fBr = 0.0;
    double fSeed = 12345.0, fGain = 0.0, fWidth = 1.0, fNl = 0.0, fNr = 0.0;
    int fType = 0;
};

} // namespace nullnoise
