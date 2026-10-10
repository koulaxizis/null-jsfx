/*
 * NULL JSFX - Formant Shift, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/formant_shift.jsfx, computed in double like EEL2: three
 * RBJ band-passes (Q 2.5) at 300/850/2250 Hz times shift = 2^((pos-0.5)*2), blended with the
 * dry signal as dry*x + wet*(1.5*F1 + 1.2*F2 + 0.8*F3), wet = min(0.8, 1.2*|2*pos - 1|):
 * dry at the centre (no shift), 0.8 towards both ends.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullformant {

static constexpr double kPi = 3.14159265358979; // the JSFX's own PI (not $pi)

struct Biquad {
    double b0, b1, b2, a1, a2;
};

struct Coeffs {
    double shift, cf[3], wet, dry;
    Biquad f[3];
};

// exactly as in the JSFX @slider section
inline Coeffs coeffsFor(double slider, double srate) noexcept
{
    static constexpr double base[3] = { 300.0, 850.0, 2250.0 };
    static constexpr double lo[3] = { 80.0, 200.0, 400.0 };
    Coeffs c;
    const double pos = slider / 100.0;
    c.shift = std::pow(2.0, (pos - 0.5) * 2.0);
    for (int i = 0; i < 3; ++i) {
        double cf = base[i] * c.shift;
        if (cf < lo[i]) cf = lo[i];
        c.cf[i] = cf;
    }
    for (int i = 0; i < 3; ++i)
        if (c.cf[i] > srate / 2.0 - 200.0) c.cf[i] = srate / 2.0 - 200.0;

    double wet = std::fabs(pos - 0.5) * 2.0;
    wet = wet * 1.2;
    if (wet > 0.8) wet = 0.8;
    c.wet = wet;
    c.dry = 1.0 - wet;

    const double q = 2.5;
    for (int i = 0; i < 3; ++i) {
        const double w0 = 2.0 * kPi * c.cf[i] / srate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0inv = 1.0 / (1.0 + alpha);
        c.f[i].b0 = a0inv * alpha;
        c.f[i].b1 = 0.0;
        c.f[i].b2 = -a0inv * alpha;
        c.f[i].a1 = -2.0 * a0inv * std::cos(w0);
        c.f[i].a2 = a0inv * (1.0 - alpha);
    }
    return c;
}

// magnitude (dB) at frequency f of dry + wet*(1.5*F1 + 1.2*F2 + 0.8*F3)
inline double magnitudeDb(double f, double slider, double srate) noexcept
{
    static constexpr double gain[3] = { 1.5, 1.2, 0.8 };
    const Coeffs c = coeffsFor(slider, srate);
    const double w = 2.0 * kPi * std::fmin(f, srate * 0.499) / srate;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    double hr = c.dry, hi = 0.0;
    for (int i = 0; i < 3; ++i) {
        const Biquad& b = c.f[i];
        // z^-1 = cos w - j sin w
        const double nr = b.b0 + b.b1 * c1 + b.b2 * c2, ni = -b.b1 * s1 - b.b2 * s2;
        const double dr = 1.0 + b.a1 * c1 + b.a2 * c2, di = -b.a1 * s1 - b.a2 * s2;
        const double dd = dr * dr + di * di;
        hr += c.wet * gain[i] * (nr * dr + ni * di) / dd;
        hi += c.wet * gain[i] * (ni * dr - nr * di) / dd;
    }
    return 10.0 * std::log10(std::fmax(hr * hr + hi * hi, 1e-10));
}

struct State {
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    double run(const Biquad& b, double x) noexcept
    {
        const double y = b.b0 * x + b.b1 * x1 + b.b2 * x2 - b.a1 * y1 - b.a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

class FormantShiftProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; update(); }
    void setAmount(double v) noexcept { fSlider = v; update(); }
    void reset() noexcept
    {
        for (int i = 0; i < 3; ++i)
            fL[i] = fR[i] = State();
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const Coeffs& c = fC;
        for (uint32_t i = 0; i < frames; ++i) {
            const double xl = inL[i], xr = inR[i];
            const double o1l = fL[0].run(c.f[0], xl), o1r = fR[0].run(c.f[0], xr);
            const double o2l = fL[1].run(c.f[1], xl), o2r = fR[1].run(c.f[1], xr);
            const double o3l = fL[2].run(c.f[2], xl), o3r = fR[2].run(c.f[2], xr);
            outL[i] = static_cast<T>(xl * c.dry + (o1l * 1.5 + o2l * 1.2 + o3l * 0.8) * c.wet);
            outR[i] = static_cast<T>(xr * c.dry + (o1r * 1.5 + o2r * 1.2 + o3r * 0.8) * c.wet);
        }
    }

private:
    void update() noexcept { fC = coeffsFor(fSlider, fSampleRate); }

    double fSampleRate = 48000.0, fSlider = 50.0;
    Coeffs fC = coeffsFor(50.0, 48000.0);
    State fL[3], fR[3];
};

} // namespace nullformant
