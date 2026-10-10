/*
 * NULL JSFX - Exciter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/exciter.jsfx, computed in double like EEL2. Per channel:
 *
 *   h = HP3k_pre(x) * drive
 *   d = h + 0.3*h*h, clamped to [-3, 3], then d*(27 + d*d)/(27 + 9*d*d)
 *   y = x + HP3k_post(d) * blend
 *
 * with drive = 1 + 9*amount, blend = 0.6*amount/sqrt(drive), amount = Amount/100 and HP3k a
 * 2nd-order RBJ high-pass at 3 kHz (Q 0.707).
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullexciter {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Coeffs {
    double hb0, hb1, ha1, ha2, drive, blend;
};

// exactly as in the JSFX @slider section
inline Coeffs coeffsFor(double slider, double srate) noexcept
{
    Coeffs k;
    const double amount = slider / 100.0;
    const double w = 2.0 * kPi * 3000.0 / srate;
    const double c = std::cos(w);
    const double al = std::sin(w) / (2.0 * 0.707);
    const double n = 1.0 / (1.0 + al);
    k.hb0 = (1.0 + c) / 2.0 * n;
    k.hb1 = -(1.0 + c) * n;
    k.ha1 = -2.0 * c * n;
    k.ha2 = (1.0 - al) * n;
    k.drive = 1.0 + amount * 9.0;
    k.blend = amount * 0.6 / std::sqrt(k.drive);
    return k;
}

// the asymmetric soft clip of excite(); output stays within [-1, 1]
inline double shape(double h) noexcept
{
    double d = h + 0.3 * h * h;
    d = std::fmax(-3.0, std::fmin(3.0, d));
    return d * (27.0 + d * d) / (27.0 + 9.0 * d * d);
}

struct HighPass {
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    double run(const Coeffs& k, double x) noexcept
    {
        const double y = k.hb0 * x + k.hb1 * x1 + k.hb0 * x2 - k.ha1 * y1 - k.ha2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

struct Channel {
    HighPass pre, post;

    double excite(const Coeffs& k, double x) noexcept
    {
        const double h = pre.run(k, x) * k.drive;
        const double d = shape(h);
        return x + post.run(k, d) * k.blend;
    }
};

class ExciterProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; update(); }
    void setAmount(double v) noexcept { fSlider = v; update(); }
    void reset() noexcept { fL = Channel(); fR = Channel(); }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i) {
            const double xl = inL[i], xr = inR[i];
            outL[i] = static_cast<T>(fL.excite(fC, xl));
            outR[i] = static_cast<T>(fR.excite(fC, xr));
        }
    }

private:
    void update() noexcept { fC = coeffsFor(fSlider, fSampleRate); }

    double fSampleRate = 48000.0, fSlider = 30.0;
    Coeffs fC = coeffsFor(30.0, 48000.0);
    Channel fL, fR;
};

} // namespace nullexciter
