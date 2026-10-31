/*
 * NULL JSFX - Tone, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/tone.jsfx, computed in double in the EEL2 order: two
 * first-order shelves built from one-pole low-passes at the bass and treble corners,
 * y = (x + (gb - 1) * lowBass + (gt - 1) * (x - lowTreble)) * go.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulltone {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    double bass = 0.0, treble = 0.0, bassFreq = 100.0, trebleFreq = 8000.0, output = 0.0;
};

struct Coeffs {
    double gb = 1.0, gt = 1.0, go = 1.0, cb = 0.0, ct = 0.0;
};

// the @slider code of tone.jsfx
inline Coeffs coeffsFor(const Params& p, double sr) noexcept
{
    Coeffs c;
    c.gb = std::pow(10.0, p.bass / 20.0);
    c.gt = std::pow(10.0, p.treble / 20.0);
    c.go = std::pow(10.0, p.output / 20.0);
    c.cb = std::exp(-2.0 * kPi * std::fmin(p.bassFreq, 0.45 * sr) / sr);
    c.ct = std::exp(-2.0 * kPi * std::fmin(p.trebleFreq, 0.45 * sr) / sr);
    return c;
}

// the response in dB at frequency f (tn_db in tone_gfx.jsfx)
inline double magnitudeDb(const Coeffs& c, double f, double sr) noexcept
{
    const double w = 2.0 * kPi * std::fmin(f, sr * 0.499) / sr;
    const double cw = std::cos(w), sw = std::sin(w);
    double d = 1.0 - 2.0 * c.cb * cw + c.cb * c.cb;
    const double lbr = (1.0 - c.cb) * (1.0 - c.cb * cw) / d;
    const double lbi = -(1.0 - c.cb) * c.cb * sw / d;
    d = 1.0 - 2.0 * c.ct * cw + c.ct * c.ct;
    const double ltr = (1.0 - c.ct) * (1.0 - c.ct * cw) / d;
    const double lti = -(1.0 - c.ct) * c.ct * sw / d;
    const double hr = 1.0 + (c.gb - 1.0) * lbr + (c.gt - 1.0) * (1.0 - ltr);
    const double hi = (c.gb - 1.0) * lbi - (c.gt - 1.0) * lti;
    return 10.0 * std::log10(std::fmax((hr * hr + hi * hi) * c.go * c.go, 0.0000000001));
}

class ToneProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        fC = coeffsFor(fP, fSr);
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fC = coeffsFor(fP, fSr);
    }

    void reset() noexcept { fLbl = fLbr = fLtl = fLtr = 0.0; }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames) noexcept
    {
        const double cb = fC.cb, ct = fC.ct, gb = fC.gb, gt = fC.gt, go = fC.go;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double x0 = inL[i], x1 = inR[i];
            fLbl = (1.0 - cb) * x0 + cb * fLbl;
            fLtl = (1.0 - ct) * x0 + ct * fLtl;
            fLbr = (1.0 - cb) * x1 + cb * fLbr;
            fLtr = (1.0 - ct) * x1 + ct * fLtr;
            outL[i] = float((x0 + (gb - 1.0) * fLbl + (gt - 1.0) * (x0 - fLtl)) * go);
            outR[i] = float((x1 + (gb - 1.0) * fLbr + (gt - 1.0) * (x1 - fLtr)) * go);
        }
    }

private:
    double fSr = 48000.0;
    Params fP;
    Coeffs fC;
    double fLbl = 0.0, fLbr = 0.0, fLtl = 0.0, fLtr = 0.0;
};

} // namespace nulltone
