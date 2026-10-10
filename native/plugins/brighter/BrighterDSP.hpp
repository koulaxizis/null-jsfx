/*
 * NULL JSFX - Brighter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/brighter.jsfx, computed in double like EEL2:
 *
 *   boost_lin = exp(Amount/100 * 9 * DB2LIN)
 *   hp_alpha  = 1 - exp(-2*PI*6000/srate)
 *   lp        = lp + hp_alpha * (x - lp)
 *   y         = x + (x - lp) * (boost_lin - 1)
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullbrighter {

static constexpr double kPi     = 3.14159265358979;       // the JSFX's own PI (not $pi)
static constexpr double kDb2Lin = 0.11512925464970228420; // ln(10)/20
static constexpr double kMaxDb  = 9.0;
static constexpr double kCornerHz = 6000.0;

struct Coeffs {
    double boostLin, alpha;
};

// exactly as in the JSFX @slider section
inline Coeffs coeffsFor(double slider, double srate) noexcept
{
    const double amount = slider / 100.0;
    const double boostDb = amount * kMaxDb;
    return { std::exp(boostDb * kDb2Lin), 1.0 - std::exp(-2.0 * kPi * kCornerHz / srate) };
}

// magnitude (dB) at frequency f: H = 1 + (1 - LP) * (g - 1), LP = a / (1 - (1-a) z^-1)
inline double magnitudeDb(double f, double slider, double srate) noexcept
{
    const Coeffs k = coeffsFor(slider, srate);
    const double w = 2.0 * kPi * std::fmin(f, srate * 0.499) / srate;
    const double b = 1.0 - k.alpha;
    const double dr = 1.0 - b * std::cos(w), di = b * std::sin(w);
    const double dd = dr * dr + di * di;
    const double lr = k.alpha * dr / dd, li = -k.alpha * di / dd;
    const double hr = 1.0 + (1.0 - lr) * (k.boostLin - 1.0);
    const double hi = -li * (k.boostLin - 1.0);
    return 10.0 * std::log10(std::fmax(hr * hr + hi * hi, 1e-10));
}

class BrighterProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; update(); }
    void setAmount(double v) noexcept { fSlider = v; update(); }
    void reset() noexcept { fLpL = fLpR = 0.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const double a = fC.alpha, g1 = fC.boostLin - 1.0;
        double lpL = fLpL, lpR = fLpR;
        for (uint32_t i = 0; i < frames; ++i) {
            const double xl = inL[i], xr = inR[i];
            lpL = lpL + a * (xl - lpL);
            lpR = lpR + a * (xr - lpR);
            const double hl = xl - lpL, hr = xr - lpR;
            outL[i] = static_cast<T>(xl + hl * g1);
            outR[i] = static_cast<T>(xr + hr * g1);
        }
        fLpL = lpL;
        fLpR = lpR;
    }

private:
    void update() noexcept { fC = coeffsFor(fSlider, fSampleRate); }

    double fSampleRate = 48000.0, fSlider = 50.0;
    Coeffs fC = coeffsFor(50.0, 48000.0);
    double fLpL = 0.0, fLpR = 0.0;
};

} // namespace nullbrighter
