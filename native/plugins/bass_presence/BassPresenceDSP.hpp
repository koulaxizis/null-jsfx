/*
 * NULL JSFX - Bass Presence, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/bass_presence.jsfx (computed in double, like EEL2): the mono
 * lows below 120 Hz (two RBJ low-passes) drive a curve that makes odd and even harmonics; only
 * those harmonics (two 180 Hz high-passes and an 1.8 kHz low-pass) are added to both channels,
 * so small speakers that cannot play the fundamental still play the bass note's harmonics.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullbasspresence {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

// an RBJ biquad (normalised), transposed direct form II, with the JSFX's denormal guard
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void lowpass(double f, double q, double sr) noexcept
    {
        const double w = 2.0 * kPi * std::min(f, 0.45 * sr) / sr, al = std::sin(w) / (2.0 * q);
        b0 = (1.0 - std::cos(w)) / 2.0 / (1.0 + al); b1 = (1.0 - std::cos(w)) / (1.0 + al); b2 = b0;
        a1 = -2.0 * std::cos(w) / (1.0 + al); a2 = (1.0 - al) / (1.0 + al);
    }
    void highpass(double f, double q, double sr) noexcept
    {
        const double w = 2.0 * kPi * std::min(f, 0.45 * sr) / sr, al = std::sin(w) / (2.0 * q);
        b0 = (1.0 + std::cos(w)) / 2.0 / (1.0 + al); b1 = -(1.0 + std::cos(w)) / (1.0 + al); b2 = b0;
        a1 = -2.0 * std::cos(w) / (1.0 + al); a2 = (1.0 - al) / (1.0 + al);
    }
    double tick(double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::fabs(z1) < 0.000000000000000000001) z1 = 0.0;
        if (std::fabs(z2) < 0.000000000000000000001) z2 = 0.0;
        return y;
    }
};

struct Params {
    double a = 0.4, k = 6.0, g = 2.4;
};

// @slider
inline Params paramsFor(double slider) noexcept
{
    Params p;
    p.a = slider / 100.0;
    p.k = 2.0 + 10.0 * p.a;
    p.g = 6.0 * p.a;
    return p;
}

// bp_curve: a soft clip (odd harmonics) plus a smooth rectifier (even ones), over the drive
inline double curve(const Params& p, double x) noexcept
{
    const double v = p.k * x;
    return (0.6 * v / std::sqrt(1.0 + v * v) + 0.8 * (std::sqrt(1.0 + v * v) - 1.0)) / p.k;
}

class BassPresenceProcessor {
public:
    // @init
    void setSampleRate(double sr) noexcept
    {
        fLo = fLo2 = fHp = fHp2 = fLp = Biquad();
        fLo.lowpass(120.0, 0.707, sr);
        fLo2.lowpass(120.0, 0.707, sr);
        fHp.highpass(180.0, 0.707, sr);
        fHp2.highpass(180.0, 0.707, sr);
        fLp.lowpass(1800.0, 0.707, sr);
    }

    void setAmount(double v) noexcept { fP = paramsFor(v); }
    const Params& params() const noexcept { return fP; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            if (fP.a > 0.0)
            {
                const double x = fLo2.tick(fLo.tick((l + r) * 0.5));
                const double h = fLp.tick(fHp2.tick(fHp.tick(curve(fP, x)))) * fP.g;
                l += h;
                r += h;
            }
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    Params fP = paramsFor(40.0);
    Biquad fLo, fLo2, fHp, fHp2, fLp;
};

} // namespace nullbasspresence
