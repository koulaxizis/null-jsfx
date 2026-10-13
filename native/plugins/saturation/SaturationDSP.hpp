/*
 * NULL JSFX - Saturation, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/saturation.jsfx, computed in double like EEL2: the input is
 * driven (1 + 3 x Drive) into one of six shapers (Tape, Tube, Overdrive, Fuzz, Bitcrush,
 * Waveshaper), the wet signal gets a tilt EQ around 800 Hz (Tone, +-12 dB), then a dry/wet mix.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullsaturation {

// the shaper for mode 0..5 with drive 0..1 (in = already driven input), as in @sample
inline double shape(double in, int mode, double drive) noexcept
{
    double wet;
    if (mode == 0)
    {
        in = std::fmax(-1.0 / 0.6, std::fmin(1.0 / 0.6, in));
        wet = in * (1.0 - std::fabs(in) * 0.3);
    }
    else if (mode == 1)
    {
        wet = in - (in * in * in) / 3.0;
        if (wet > 1.0)
            wet = 1.0;
        else if (wet < -1.0)
            wet = -1.0;
    }
    else if (mode == 2)
    {
        const double a = in < 0.0 ? -in : in;
        wet = a > 1.0 ? (in < 0.0 ? -1.0 : 1.0) : in;
        if (wet > 0.5)
            wet = 1.0 - (1.0 - wet) * (1.0 - wet);
        if (wet < -0.5)
            wet = -1.0 + (1.0 + wet) * (1.0 + wet);
    }
    else if (mode == 3)
    {
        const double a = in < 0.0 ? -in : in;
        wet = a > 0.1 ? (in > 0.0 ? 1.0 : -1.0) : 0.0;
    }
    else if (mode == 4)
    {
        double bits = 8.0 - std::floor(drive * 6.0);
        if (bits < 2.0)
            bits = 2.0;
        const double quants = std::pow(2.0, bits) - 1.0;
        wet = std::floor(in * quants + 0.5) / quants;
    }
    else
    {
        wet = in;
        if (wet > 1.0)
            wet = 2.0 - wet;
        if (wet < -1.0)
            wet = -2.0 - wet;
        if (wet > 1.0)
            wet = 2.0 - wet;
        if (wet < -1.0)
            wet = -2.0 - wet;
    }
    return wet;
}

class SaturationProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fToneL = fToneR = 0.0;
        fAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979 * 800.0 / fSr);
    }

    // @slider
    void setParams(double drivePct, double toneDb, double mixPct, int mode)
    {
        fDrive = drivePct / 100.0;
        fMix = mixPct / 100.0;
        fMode = mode;
        fAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979 * 800.0 / fSr);
        fHi = std::pow(10.0, toneDb / 40.0);
        fLo = 1.0 / fHi;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double dl = inL[i], dr = inR[i];
            double wl = shape(dl * (1.0 + fDrive * 3.0), fMode, fDrive);
            double wr = shape(dr * (1.0 + fDrive * 3.0), fMode, fDrive);
            fToneL += fAlpha * (wl - fToneL);
            fToneR += fAlpha * (wr - fToneR);
            wl = fToneL * fLo + (wl - fToneL) * fHi;
            wr = fToneR * fLo + (wr - fToneR) * fHi;
            outL[i] = static_cast<T>(dl * (1.0 - fMix) + wl * fMix);
            outR[i] = static_cast<T>(dr * (1.0 - fMix) + wr * fMix);
        }
    }

private:
    double fSr = 48000.0, fAlpha = 0.0, fToneL = 0.0, fToneR = 0.0;
    double fDrive = 0.3, fMix = 0.5, fHi = 1.0, fLo = 1.0;
    int fMode = 0;
};

} // namespace nullsaturation
