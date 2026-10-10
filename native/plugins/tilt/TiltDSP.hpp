/*
 * NULL JSFX - Tilt, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Bit-for-bit port of DATA/Effects/null_jsfx/tilt.jsfx (computed in double, like EEL2):
 *
 *   c      = exp(-2*pi*700/srate)
 *   g_low  = 10^(-t*6/20),  g_high = 10^(t*6/20),  t = Amount/100
 *   lp     = (1-c)*x + c*lp
 *   y      = lp*g_low + (x-lp)*g_high
 *
 * The only addition is a ~10 ms one-pole glide on g_low/g_high so automation does not
 * zipper. The glide snaps to the exact target once it is within 1e-9, so at rest the
 * gains (and therefore the output) are identical to the JSFX's.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulltilt {

static constexpr double kPi          = 3.141592653589793; // EEL2 $pi
static constexpr double kPivotHz     = 700.0;
static constexpr double kMaxDb       = 6.0;
static constexpr double kSmoothSec   = 0.010;
static constexpr double kSnapEpsilon = 1e-9;

struct TiltGains {
    double low, high;
};

// Gains exactly as computed in the JSFX @slider section.
inline TiltGains gainsForAmount(double amount) noexcept
{
    const double t = amount / 100.0;
    return { std::pow(10.0, -t * kMaxDb / 20.0), std::pow(10.0, t * kMaxDb / 20.0) };
}

inline double lowpassCoeff(double sampleRate) noexcept
{
    return std::exp(-2.0 * kPi * kPivotHz / sampleRate);
}

// Magnitude (dB) of the tilt filter at frequency f for the given amount/sample rate.
// H(z) = gl*LP(z) + gh*(1-LP(z)),  LP(z) = (1-c)/(1 - c z^-1)
inline double magnitudeDb(double f, double amount, double sampleRate) noexcept
{
    const TiltGains g = gainsForAmount(amount);
    const double c = lowpassCoeff(sampleRate);
    const double w = 2.0 * kPi * f / sampleRate;
    // denominator d = 1 - c e^{-jw}
    const double dr = 1.0 - c * std::cos(w);
    const double di = c * std::sin(w);
    const double dd = dr * dr + di * di;
    // LP = (1-c)/d = (1-c) * conj(d)/|d|^2
    const double lr = (1.0 - c) * dr / dd;
    const double li = -(1.0 - c) * di / dd;
    const double hr = g.low * lr + g.high * (1.0 - lr);
    const double hi = g.low * li - g.high * li;
    return 10.0 * std::log10(hr * hr + hi * hi + 1e-300);
}

class TiltProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSampleRate = sr;
        fC = lowpassCoeff(sr);
        fSmoothK = 1.0 - std::exp(-1.0 / (kSmoothSec * sr));
    }

    void setAmount(double amount) noexcept
    {
        fAmount = amount;
        const TiltGains g = gainsForAmount(amount);
        fTargetLow = g.low;
        fTargetHigh = g.high;
    }

    double getAmount() const noexcept { return fAmount; }

    // Clear filter state and jump gains to target (JSFX @init + @slider).
    void reset() noexcept
    {
        fLpL = fLpR = 0.0;
        fGainLow = fTargetLow;
        fGainHigh = fTargetHigh;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const double c = fC;
        const double a = 1.0 - c;
        double lpL = fLpL, lpR = fLpR;
        double gl = fGainLow, gh = fGainHigh;
        const double tl = fTargetLow, th = fTargetHigh;
        const bool gliding = (gl != tl) || (gh != th);

        if (!gliding) {
            for (uint32_t i = 0; i < frames; ++i) {
                const double xl = inL[i], xr = inR[i];
                lpL = a * xl + c * lpL;
                lpR = a * xr + c * lpR;
                outL[i] = static_cast<T>(lpL * gl + (xl - lpL) * gh);
                outR[i] = static_cast<T>(lpR * gl + (xr - lpR) * gh);
            }
        } else {
            const double k = fSmoothK;
            for (uint32_t i = 0; i < frames; ++i) {
                gl += (tl - gl) * k;
                gh += (th - gh) * k;
                if (std::fabs(tl - gl) < kSnapEpsilon && std::fabs(th - gh) < kSnapEpsilon) {
                    gl = tl;
                    gh = th;
                }
                const double xl = inL[i], xr = inR[i];
                lpL = a * xl + c * lpL;
                lpR = a * xr + c * lpR;
                outL[i] = static_cast<T>(lpL * gl + (xl - lpL) * gh);
                outR[i] = static_cast<T>(lpR * gl + (xr - lpR) * gh);
            }
        }

        fLpL = lpL;
        fLpR = lpR;
        fGainLow = gl;
        fGainHigh = gh;
    }

private:
    double fSampleRate = 48000.0;
    double fC = lowpassCoeff(48000.0);
    double fSmoothK = 1.0 - std::exp(-1.0 / (kSmoothSec * 48000.0));
    double fAmount = 0.0;
    double fTargetLow = 1.0, fTargetHigh = 1.0;
    double fGainLow = 1.0, fGainHigh = 1.0;
    double fLpL = 0.0, fLpR = 0.0;
};

} // namespace nulltilt
