/*
 * NULL JSFX - Sweeper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/sweeper.jsfx, computed in double like EEL2: a TPT
 * state-variable filter, low-pass below 50 (20 kHz down to 80 Hz), high-pass above 50
 * (20 Hz up to 8 kHz), Q 0.707 rising to 2.207 at the ends; 50 bypasses the filter.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullsweeper {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Coeffs {
    int mode;          // -1 low-pass, 0 open, +1 high-pass
    double fc, q, g, k, a1, a2, a3;
};

// exactly as in the JSFX @slider section
inline Coeffs coeffsFor(double slider, double srate) noexcept
{
    Coeffs c;
    const double pos = (slider - 50.0) / 50.0;
    c.mode = pos < 0.0 ? -1 : (pos > 0.0 ? 1 : 0);
    const double d = std::fabs(pos);
    double fc = c.mode < 0 ? 20000.0 * std::pow(80.0 / 20000.0, d) : 20.0 * std::pow(8000.0 / 20.0, d);
    fc = std::fmin(fc, srate * 0.45);
    c.fc = fc;
    c.q = 0.707 + d * 1.5;
    c.g = std::tan(kPi * fc / srate);
    c.k = 1.0 / c.q;
    c.a1 = 1.0 / (1.0 + c.g * (c.g + c.k));
    c.a2 = c.g * c.a1;
    c.a3 = c.g * c.a2;
    return c;
}

// magnitude (dB) at frequency f; the TPT SVF is the bilinear transform of
// 1/(s^2 + k s + 1) (LP) and s^2/(s^2 + k s + 1) (HP) with s = j tan(pi f/srate) / g
inline double magnitudeDb(double f, double slider, double srate) noexcept
{
    const Coeffs c = coeffsFor(slider, srate);
    if (c.mode == 0)
        return 0.0;
    const double om = std::tan(kPi * std::fmin(f, srate * 0.499) / srate) / c.g;
    const double re = 1.0 - om * om, im = c.k * om;
    const double den = re * re + im * im;
    const double num = c.mode < 0 ? 1.0 : om * om * om * om;
    return 10.0 * std::log10(std::fmax(num / den, 1e-10));
}

struct Svf {
    double ic1 = 0.0, ic2 = 0.0;

    double run(const Coeffs& c, double x) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return c.mode < 0 ? v2 : x - c.k * v1 - v2;
    }
};

class SweeperProcessor {
public:
    void setSampleRate(double sr) noexcept { fSampleRate = sr; update(); }
    void setAmount(double v) noexcept { fSlider = v; update(); }
    void reset() noexcept { fL = Svf(); fR = Svf(); }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        if (fC.mode == 0) { // JSFX: mode != 0 ? (...) -- the signal passes untouched
            for (uint32_t i = 0; i < frames; ++i) {
                outL[i] = inL[i];
                outR[i] = inR[i];
            }
            return;
        }
        for (uint32_t i = 0; i < frames; ++i) {
            const double xl = inL[i], xr = inR[i];
            outL[i] = static_cast<T>(fL.run(fC, xl));
            outR[i] = static_cast<T>(fR.run(fC, xr));
        }
    }

private:
    void update() noexcept { fC = coeffsFor(fSlider, fSampleRate); }

    double fSampleRate = 48000.0, fSlider = 50.0;
    Coeffs fC = coeffsFor(50.0, 48000.0);
    Svf fL, fR;
};

} // namespace nullsweeper
