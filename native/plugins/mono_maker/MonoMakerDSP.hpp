/*
 * NULL JSFX - Mono Maker, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/mono_maker.jsfx, computed in double like EEL2 and in the
 * same order: the side signal (L-R)/2 runs through two identical Butterworth high-pass
 * stages (fc = 20 * 20^amount Hz); the part below, side - high, is removed from the
 * side (L -= low, R += low). At amount 0 the plugin passes the input through.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullmonomaker {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Coeffs {
    double amount, fc, b0, b1, b2, a1, a2;
};

// the JSFX @slider section
inline Coeffs coeffsFor(double v, double sr) noexcept
{
    Coeffs c;
    c.amount = v / 100.0;
    c.fc = 20.0 * std::pow(20.0, c.amount);
    const double w = 2.0 * kPi * c.fc / sr;
    const double k = std::tan(w / 2.0);
    const double norm = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
    c.b0 = norm; // high-pass
    c.b1 = -2.0 * c.b0;
    c.b2 = c.b0;
    c.a1 = 2.0 * (k * k - 1.0) * norm;
    c.a2 = (1.0 - std::sqrt(2.0) * k + k * k) * norm;
    return c;
}

// display: the side response in dB at f, H(z)^2 (0 dB when off)
inline double sideDb(const Coeffs& c, double f, double sr) noexcept
{
    if (!(c.amount > 0.0))
        return 0.0;
    const double w = 2.0 * kPi * std::fmin(f, sr * 0.499) / sr;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    const double nr = c.b0 + c.b1 * c1 + c.b2 * c2;
    const double ni = -(c.b1 * s1 + c.b2 * s2);
    const double dr = 1.0 + c.a1 * c1 + c.a2 * c2;
    const double di = -(c.a1 * s1 + c.a2 * s2);
    const double dd = dr * dr + di * di;
    const double hr = (nr * dr + ni * di) / dd;
    const double hi = (ni * dr - nr * di) / dd;
    const double m = hr * hr + hi * hi;
    return 10.0 * std::log10(std::fmax(m * m, 1e-10));
}

class MonoMakerProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr;
        fC = coeffsFor(fValue, sr);
    }

    void setAmount(double v) noexcept
    {
        fValue = v;
        fC = coeffsFor(v, fSr);
    }

    // JSFX @init
    void reset() noexcept { fS1 = fS2 = fS3 = fS4 = 0.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Coeffs c = fC;
        for (uint32_t i = 0; i < frames; ++i) {
            double x0 = inL[i], x1 = inR[i];
            const double side = (x0 - x1) * 0.5;
            const double y = c.b0 * side + fS1;
            fS1 = c.b1 * side - c.a1 * y + fS2;
            fS2 = c.b2 * side - c.a2 * y;
            const double high = c.b0 * y + fS3;
            fS3 = c.b1 * y - c.a1 * high + fS4;
            fS4 = c.b2 * y - c.a2 * high;
            const double low = side - high;
            if (c.amount > 0.0) {
                x0 -= low;
                x1 += low;
            }
            outL[i] = static_cast<T>(x0);
            outR[i] = static_cast<T>(x1);
        }
    }

private:
    double fSr = 48000.0, fValue = 50.0;
    Coeffs fC = coeffsFor(50.0, 48000.0);
    double fS1 = 0.0, fS2 = 0.0, fS3 = 0.0, fS4 = 0.0;
};

} // namespace nullmonomaker
