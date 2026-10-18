/*
 * NULL JSFX - Noise Generator, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/noise_generator.jsfx, computed in double like EEL2: white,
 * pink (Paul Kellet's filter) or brown (leaky integrator) noise at a level in dB, with a
 * stereo width from mono (0%) to two independent channels (100%). Low Cut / High Cut (12 dB/oct
 * Butterworth SVFs, off at 20 Hz / 20 kHz), Follow Input (the noise follows the input's envelope)
 * and Ducking (the noise dips while the input is loud), then Mix Mode: Replace (the default, the
 * input is replaced) or Add.
 * The noise source is the JSFX's Park-Miller generator in exact double arithmetic, so the
 * output matches the JSFX sample for sample.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullnoise {

class NoiseProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        for (int i = 0; i < 7; ++i)
            fPl[i] = fPr[i] = 0.0;
        fBl = fBr = 0.0;
        fSeed = 12345.0;
        fSr = sr;
        fLc1L = fLc2L = fLc1R = fLc2R = 0.0;
        fHc1L = fHc2L = fHc1R = fHc2R = 0.0;
        fFoEnv = fDkEnv = 0.0;
    }

    // @slider
    void setParams(int type, double levelDb, double widthPct, double lowCut = 20.0, double highCut = 20000.0,
                   double followPct = 0.0, double duckPct = 0.0, int mixMode = 0)
    {
        fType = type;
        fWidth = widthPct / 100.0;
        fGain = std::exp(levelDb * 0.11512925464970228420);
        fLcOn = lowCut > 20.0;
        fLcG = std::tan(kPi * lowCut / fSr);
        fLcD = 1.0 / (1.0 + kSqrt2 * fLcG + fLcG * fLcG);
        fHcOn = highCut < 20000.0;
        fHcG = std::tan(kPi * std::min(highCut, fSr * 0.45) / fSr);
        fHcD = 1.0 / (1.0 + kSqrt2 * fHcG + fHcG * fHcG);
        fFoOn = followPct > 0.0;
        fFoAmt = followPct / 100.0;
        fFoAtt = 1.0 - std::exp(-1.0 / (0.001 * fSr));
        fFoRel = 1.0 - std::exp(-1.0 / (0.06 * fSr));
        fDkOn = duckPct > 0.0;
        fDkAmt = duckPct / 100.0;
        fDkAtt = 1.0 - std::exp(-1.0 / (0.01 * fSr));
        fDkRel = 1.0 - std::exp(-1.0 / (0.25 * fSr));
        fAdd = mixMode != 0;
    }

    // inputs may be the outputs (in place): each input sample is read before it is written
    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
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
            if (fLcOn)
            {
                nl = svf(nl, fLc1L, fLc2L, fLcG, fLcD, true);
                nr = svf(nr, fLc1R, fLc2R, fLcG, fLcD, true);
            }
            if (fHcOn)
            {
                nl = svf(nl, fHc1L, fHc2L, fHcG, fHcD, false);
                nr = svf(nr, fHc1R, fHc2R, fHcG, fHcD, false);
            }
            if (fFoOn || fDkOn)
            {
                const double pk = std::max(std::fabs(xl), std::fabs(xr));
                if (fFoOn)
                {
                    fFoEnv += (pk > fFoEnv ? fFoAtt : fFoRel) * (pk - fFoEnv);
                    const double g = 1.0 - fFoAmt + fFoAmt * std::min(1.0, fFoEnv);
                    nl *= g;
                    nr *= g;
                }
                if (fDkOn)
                {
                    fDkEnv += (pk > fDkEnv ? fDkAtt : fDkRel) * (pk - fDkEnv);
                    const double g = 1.0 - fDkAmt * std::min(1.0, fDkEnv * 4.0);
                    nl *= g;
                    nr *= g;
                }
            }
            if (fAdd)
            {
                nl = xl + nl;
                nr = xr + nr;
            }
            outL[i] = static_cast<T>(nl);
            outR[i] = static_cast<T>(nr);
        }
    }

private:
    static constexpr double kPi = 3.141592653589793;
    static constexpr double kSqrt2 = 1.4142135623730951;

    // TPT state-variable filter, s^2 + sqrt(2) s + 1: the high pass (v3) or the low pass (v2)
    static double svf(double x, double& ic1, double& ic2, double g, double d, bool hp) noexcept
    {
        const double v3 = (x - ic2 - (kSqrt2 + g) * ic1) * d;
        const double v1 = g * v3 + ic1;
        const double v2 = g * v1 + ic2;
        ic1 = v1;
        ic2 = v2;
        return hp ? v3 : v2;
    }

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
    double fSr = 48000.0;
    bool fLcOn = false, fHcOn = false, fFoOn = false, fDkOn = false, fAdd = false;
    double fLcG = 0.0, fLcD = 1.0, fHcG = 0.0, fHcD = 1.0;
    double fLc1L = 0, fLc2L = 0, fLc1R = 0, fLc2R = 0, fHc1L = 0, fHc2L = 0, fHc1R = 0, fHc2R = 0;
    double fFoAmt = 0.0, fFoAtt = 0.0, fFoRel = 0.0, fFoEnv = 0.0;
    double fDkAmt = 0.0, fDkAtt = 0.0, fDkRel = 0.0, fDkEnv = 0.0;
};

} // namespace nullnoise
