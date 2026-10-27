/*
 * NULL JSFX - Reverser, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/reverser.jsfx, computed in double like EEL2: a buffer of Buffer
 * Length is written forwards while it is read backwards; the reversed signal can go through a
 * low-, band- or high-pass (RBJ, Q 0.707, state kept across changes) and is mixed with the dry.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullreverser {

static constexpr double kPi = 3.141592653589793;

// the filter as the JSFX keeps it: y = fb0 x + fb1 x1 + fb2 x2 - fa0 (fa1 y1 + fa2 y2)
struct Coeffs {
    double fa0 = 1, fa1 = 0, fa2 = 0, fb0 = 1, fb1 = 0, fb2 = 0;
    int mode = 0;
};

inline Coeffs coeffs(int mode, double freq, double sr) noexcept
{
    // below Nyquist at any sample rate, as the JSFX: freq = min(filt_freq, srate * 0.49)
    freq = std::min(freq, sr * 0.49);
    const double omega = 2 * kPi * freq / sr;
    const double cs = std::cos(omega), sn = std::sin(omega);
    const double q = 0.707;
    const double alpha = sn / (2 * q);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    if (mode == 1)
    {
        b0 = (1 - cs) * 0.5; b1 = 1 - cs; b2 = (1 - cs) * 0.5;
        a0 = 1 / (1 + alpha); a1 = -2 * cs; a2 = 1 - alpha;
    }
    else if (mode == 2)
    {
        b0 = alpha; b1 = 0; b2 = -alpha;
        a0 = 1 / (1 + alpha); a1 = -2 * cs; a2 = 1 - alpha;
    }
    else if (mode == 3)
    {
        b0 = (1 + cs) * 0.5; b1 = -(1 + cs); b2 = (1 + cs) * 0.5;
        a0 = 1 / (1 + alpha); a1 = -2 * cs; a2 = 1 - alpha;
    }
    Coeffs c;
    c.mode = mode;
    c.fa0 = a0;
    c.fb0 = b0 * a0; c.fb1 = b1 * a0; c.fb2 = b2 * a0;
    c.fa1 = a1; c.fa2 = a2;
    return c;
}

// display: the wet filter's response in dB at f, 0 when Off (rv_db in the GFX)
inline double responseDb(const Coeffs& c, double f, double sr) noexcept
{
    if (c.mode <= 0)
        return 0.0;
    const double w = 2 * kPi * std::min(f, sr * 0.499) / sr;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
    const double nr = c.fb0 + c.fb1 * c1 + c.fb2 * c2, ni = -(c.fb1 * s1 + c.fb2 * s2);
    const double dr = 1 + c.fa0 * c.fa1 * c1 + c.fa0 * c.fa2 * c2, di = -(c.fa0 * c.fa1 * s1 + c.fa0 * c.fa2 * s2);
    return 10 * std::log10(std::max((nr * nr + ni * ni) / (dr * dr + di * di), 1e-10));
}

class ReverserProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fMax = int(sr * 10);
        fL.assign(size_t(fMax), 0.0);
        fR.assign(size_t(fMax), 0.0);
        fWrite = 0;
        fRead = 0;
        fC = Coeffs();
        fSL = fSR = State();
    }

    // @slider
    void setParams(double lengthSec, int mode, double freq, double mixPct)
    {
        fSize = int64_t(fSr * lengthSec); // EEL2 (x) | 0
        if (fSize < 1)
            fSize = 1;
        fSize = std::min<int64_t>(fSize, fMax);
        fWrite = fWrite % fSize;
        fRead = fRead % fSize;
        fMix = mixPct / 100;
        fC = coeffs(mode, freq, fSr);
    }

    double writeFraction() const noexcept { return double(fWrite) / double(fSize); }
    double readFraction() const noexcept { return double(fRead) / double(fSize); }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            fL[size_t(fWrite)] = xl;
            fR[size_t(fWrite)] = xr;
            fWrite = (fWrite + 1) % fSize;

            double wl = fL[size_t(fRead)], wr = fR[size_t(fRead)];
            fRead -= 1;
            if (fRead < 0)
                fRead = fSize - 1;

            if (fC.mode > 0)
            {
                wl = filter(fSL, wl);
                wr = filter(fSR, wr);
            }
            outL[n] = static_cast<T>(xl * (1 - fMix) + wl * fMix);
            outR[n] = static_cast<T>(xr * (1 - fMix) + wr * fMix);
        }
    }

private:
    struct State { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

    double filter(State& s, double x) const noexcept
    {
        const double y = fC.fb0 * x + fC.fb1 * s.x1 + fC.fb2 * s.x2 - fC.fa0 * (fC.fa1 * s.y1 + fC.fa2 * s.y2);
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    double fSr = 48000.0, fMix = 0.5;
    std::vector<double> fL, fR;
    int64_t fMax = 1, fSize = 1, fWrite = 0, fRead = 0;
    Coeffs fC;
    State fSL, fSR;
};

} // namespace nullreverser
