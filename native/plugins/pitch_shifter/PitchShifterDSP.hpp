/*
 * NULL JSFX - Pitch Shifter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/pitch_shifter.jsfx, computed in double like EEL2: two read
 * taps sweep through a 60 ms delay line in opposite phase; a Hann crossfade hides each tap
 * while it jumps back, so the pitch changes without clicks. Blended with the dry signal (Mix).
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullpitchshifter {

// the total shift in semitones
inline double totalShift(double octave, double semitones, double fineCents) noexcept
{
    return octave * 12.0 + semitones + fineCents / 100.0;
}

class PitchShifterProcessor {
public:
    // @init: a 60 ms window, cleared buffers
    void setSampleRate(double sr)
    {
        fSr = sr;
        fWin = std::floor(sr * 0.06);
        fSize = int(fWin) + 4;
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fPh = 0.0;
    }

    // @slider
    void setParams(double octave, double semitones, double fineCents, double mixPct)
    {
        fMix = mixPct / 100.0;
        const double pf = std::pow(2.0, totalShift(octave, semitones, fineCents) / 12.0);
        fPhInc = (1.0 - pf) / fWin;
    }

    double tapPhase() const noexcept { return fPh; }   // tap 1, 0..1 of the window
    double phaseInc() const noexcept { return fPhInc; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            fL[size_t(fW)] = xl;
            fR[size_t(fW)] = xr;

            fPh += fPhInc;
            if (fPh >= 1.0)
                fPh -= 1.0;
            if (fPh < 0.0)
                fPh += 1.0;
            double ph2 = fPh + 0.5;
            if (ph2 >= 1.0)
                ph2 -= 1.0;

            const double d1 = 1.0 + fPh * fWin;
            const double d2 = 1.0 + ph2 * fWin;
            const double g1 = 0.5 - 0.5 * std::cos(2.0 * M_PI * fPh);
            const double g2 = 1.0 - g1;

            const double sl = readTap(fL, d1) * g1 + readTap(fL, d2) * g2;
            const double sr = readTap(fR, d1) * g1 + readTap(fR, d2) * g2;

            if (++fW >= fSize)
                fW = 0;

            outL[i] = static_cast<T>(xl * (1.0 - fMix) + sl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + sr * fMix);
        }
    }

private:
    double readTap(const std::vector<double>& buf, double delay) const
    {
        double rp = double(fW) - delay;
        if (rp < 0.0)
            rp += double(fSize);
        const double fi = std::floor(rp), fr = rp - fi;
        const int i = int(fi);
        int i2 = i + 1;
        if (i2 >= fSize)
            i2 -= fSize;
        return buf[size_t(i)] + (buf[size_t(i2)] - buf[size_t(i)]) * fr;
    }

    double fSr = 48000.0, fWin = 2880.0, fPh = 0.0, fPhInc = 0.0, fMix = 0.5;
    std::vector<double> fL, fR;
    int fSize = 1, fW = 0;
};

} // namespace nullpitchshifter
