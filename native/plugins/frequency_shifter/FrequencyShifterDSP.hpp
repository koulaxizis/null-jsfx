/*
 * NULL JSFX - Frequency Shifter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/frequency_shifter.jsfx, computed in double like EEL2: a Bode
 * shifter per channel. A Hilbert transform (two chains of four allpasses in z^2, Niemitalo
 * design) gives the quadrature pair, a sine/cosine oscillator moves every frequency by Shift
 * (the right channel by Shift + Stereo Offset). Feedback is high-passed and softly limited.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullfreqshift {

static constexpr double kPi = 3.141592653589793;

// first-order-in-z^2 allpass section: y = a*(x + y[n-2]) - x[n-2]
struct Allpass {
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    double operator()(double x, double a) noexcept
    {
        const double y = a * (x + y2) - x2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

struct Hilbert {
    Allpass a1, a2, a3, a4, b1, b2, b3, b4;
    double im = 0, imd = 0;
    // returns the real part, the imaginary part goes to im
    double operator()(double x) noexcept
    {
        const double re = a4(a3(a2(a1(x, 0.479401), 0.876218), 0.976597), 0.997499);
        const double i = b4(b3(b2(b1(x, 0.161758), 0.733029), 0.945349), 0.990598);
        // the imaginary path is one sample behind the real path
        im = imd;
        imd = i;
        return re;
    }
};

struct Shifter {
    Hilbert h;
    double ph = 0, fb = 0, dc = 0;
    double operator()(double x, double inc, double feedback) noexcept
    {
        ph += inc;
        if (ph >= 1) ph -= 1;
        if (ph < 0) ph += 1;
        double f = fb * feedback;
        dc = dc * 0.995 + f * 0.005;
        f -= dc;
        f = f / (1 + std::fabs(f));
        const double re = h(x + f);
        fb = re * std::cos(2 * kPi * ph) + h.im * std::sin(2 * kPi * ph);
        return fb;
    }
};

class FreqShiftProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fL = Shifter();
        fR = Shifter();
    }

    // @slider
    void setParams(double shift, double offset, double feedbackPct, double mixPct)
    {
        fIncL = shift / fSr;
        fIncR = (shift + offset) / fSr;
        fFeedback = feedbackPct / 100;
        fMix = mixPct / 100;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            const double wl = fL(xl, fIncL, fFeedback);
            const double wr = fR(xr, fIncR, fFeedback);
            outL[n] = static_cast<T>(xl * (1 - fMix) + wl * fMix);
            outR[n] = static_cast<T>(xr * (1 - fMix) + wr * fMix);
        }
    }

private:
    double fSr = 48000.0, fIncL = 0, fIncR = 0, fFeedback = 0, fMix = 0.5;
    Shifter fL, fR;
};

} // namespace nullfreqshift
