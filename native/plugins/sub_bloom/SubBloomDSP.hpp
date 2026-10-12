/*
 * NULL JSFX - Sub Bloom, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/sub_bloom.jsfx, computed in double like EEL2: per channel the
 * input is band-limited to the bass (high-pass at Source / 4, 24 dB/oct low-pass at Source),
 * its zero crossings (Schmitt trigger) clock a flip-flop for one octave down and a second one
 * for two octaves down; the squares are shaped by the band's envelope and smoothed into a sub.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullsubbloom {

static constexpr double kPi = 3.141592653589793;

struct Biquad {
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    // bq_set: type 0 low-pass, 1 high-pass, Q 0.7071
    void set(int type, double f, double sr) noexcept
    {
        const double w = 2 * kPi * (std::min(f, sr * 0.45) / sr);
        const double cs = std::cos(w), al = std::sin(w) / (2 * 0.7071);
        const double n = 1 / (1 + al);
        if (type == 0)
        {
            b0 = (1 - cs) * 0.5 * n; b1 = (1 - cs) * n; b2 = b0;
        }
        else
        {
            b0 = (1 + cs) * 0.5 * n; b1 = -(1 + cs) * n; b2 = b0;
        }
        a1 = -2 * cs * n; a2 = (1 - al) * n;
    }
    double operator()(double x) noexcept
    {
        // grouped the way EEL2 evaluates "b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2" (bit-exact)
        const double y = (b0 * x + b1 * x1) + ((b2 * x2 - a1 * y1) - a2 * y2);
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

struct Channel {
    Biquad hp, lpa, lpb, s1a, s1b, s2a, s2b;
    double env = 0, arm = 0, ff1 = 0, ff2 = 0;

    void setup(double f, double sr) noexcept
    {
        hp.set(1, f * 0.25, sr);
        lpa.set(0, f, sr); lpb.set(0, f, sr);
        s1a.set(0, f * 0.5, sr); s1b.set(0, f * 0.5, sr);
        s2a.set(0, f * 0.25, sr); s2b.set(0, f * 0.25, sr);
    }

    // band-limit, track, divide, smooth; returns the sub sample
    double sub(double x, double envAtt, double envRel, int octMode) noexcept
    {
        const double band = lpb(lpa(hp(x)));
        env += (std::fabs(band) - env) * (std::fabs(band) > env ? envAtt : envRel);
        const double th = std::max(0.0005, env * 0.3);
        if (band < -th)
            arm = 1;
        if (band > th && arm != 0)
        {
            arm = 0;
            ff1 = ff1 > 0 ? -1 : 1;
            if (ff1 > 0)
                ff2 = ff2 > 0 ? -1 : 1;
        }
        if (ff1 == 0) ff1 = 1;
        if (ff2 == 0) ff2 = 1;
        const double sq1 = s1b(s1a(ff1 * env * 1.5708));
        const double sq2 = s2b(s2a(ff2 * env * 1.5708));
        return octMode == 0 ? sq1 : octMode == 1 ? sq2 : (sq1 + sq2) * 0.7071;
    }
};

class SubBloomProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fL = Channel();
        fR = Channel();
        fEnvAtt = 1 - std::exp(-1 / (0.005 * sr));
        fEnvRel = 1 - std::exp(-1 / (0.06 * sr));
    }

    // @slider
    void setParams(double srcFreq, double subPct, int octMode, double origPct, double outDb)
    {
        fSubLevel = subPct / 100;
        fOct = octMode;
        fOrig = origPct / 100;
        fOut = std::pow(10.0, outDb / 20);
        fL.setup(srcFreq, fSr);
        fR.setup(srcFreq, fSr);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            const double sl = fL.sub(xl, fEnvAtt, fEnvRel, fOct) * fSubLevel;
            const double sr = fR.sub(xr, fEnvAtt, fEnvRel, fOct) * fSubLevel;
            outL[n] = static_cast<T>((xl * fOrig + sl) * fOut);
            outR[n] = static_cast<T>((xr * fOrig + sr) * fOut);
        }
    }

private:
    double fSr = 48000.0, fEnvAtt = 0, fEnvRel = 0, fSubLevel = 0.5, fOrig = 0.8, fOut = 1;
    int fOct = 0;
    Channel fL, fR;
};

} // namespace nullsubbloom
