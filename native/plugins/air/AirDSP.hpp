/*
 * NULL JSFX - Air, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/air.jsfx, computed in double in the EEL2 order: an RBJ
 * high shelf with slope 0.5, gain 9 dB * Amount/100, corner 16 kHz - 6 kHz * Amount/100
 * (held below 0.45 * srate), run as a direct form I biquad per channel.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullair {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Coeffs {
    double f = 16000.0, gdb = 0.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// the @slider code of air.jsfx
inline Coeffs coeffsFor(double amount, double sr) noexcept
{
    Coeffs c;
    const double t = amount / 100.0;
    c.f = std::fmin(16000.0 - 6000.0 * t, 0.45 * sr);
    c.gdb = 9.0 * t;
    const double A = std::pow(10.0, c.gdb / 40.0);
    const double w = 2.0 * kPi * c.f / sr;
    const double cw = std::cos(w);
    const double sa = std::sqrt(A) * std::sin(w) * std::sqrt((A + 1.0 / A) * (1.0 / 0.5 - 1.0) + 2.0);
    const double a0 = (A + 1.0) - (A - 1.0) * cw + sa;
    c.b0 = A * ((A + 1.0) + (A - 1.0) * cw + sa) / a0;
    c.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
    c.b2 = A * ((A + 1.0) + (A - 1.0) * cw - sa) / a0;
    c.a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
    c.a2 = ((A + 1.0) - (A - 1.0) * cw - sa) / a0;
    return c;
}

// the shelf's response in dB at frequency f (air_db in air_gfx.jsfx)
inline double magnitudeDb(const Coeffs& c, double f, double sr) noexcept
{
    const double w = 2.0 * kPi * std::fmin(f, sr * 0.499) / sr;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    const double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = -(c.b1 * s1 + c.b2 * s2);
    const double dr = 1.0 + c.a1 * c1 + c.a2 * c2, di = -(c.a1 * s1 + c.a2 * s2);
    return 10.0 * std::log10(std::fmax((nr * nr + ni * ni) / std::fmax(dr * dr + di * di, 1e-10), 1e-10));
}

class AirProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; fC = coeffsFor(fAmount, sr); }
    void setAmount(double amount) noexcept { fAmount = amount; fC = coeffsFor(amount, fSampleRate); }
    const Coeffs& coeffs() const noexcept { return fC; }

    void reset() noexcept { fXl1 = fXl2 = fYl1 = fYl2 = fXr1 = fXr2 = fYr1 = fYr2 = 0.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Coeffs c = fC;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            const double yl = c.b0 * xl + c.b1 * fXl1 + c.b2 * fXl2 - c.a1 * fYl1 - c.a2 * fYl2;
            fXl2 = fXl1; fXl1 = xl;
            fYl2 = fYl1; fYl1 = yl;
            const double yr = c.b0 * xr + c.b1 * fXr1 + c.b2 * fXr2 - c.a1 * fYr1 - c.a2 * fYr2;
            fXr2 = fXr1; fXr1 = xr;
            fYr2 = fYr1; fYr1 = yr;
            outL[i] = static_cast<T>(yl);
            outR[i] = static_cast<T>(yr);
        }
    }

private:
    double fSampleRate = 48000.0, fAmount = 0.0;
    Coeffs fC = coeffsFor(0.0, 48000.0);
    double fXl1 = 0.0, fXl2 = 0.0, fYl1 = 0.0, fYl2 = 0.0;
    double fXr1 = 0.0, fXr2 = 0.0, fYr1 = 0.0, fYr2 = 0.0;
};

} // namespace nullair
