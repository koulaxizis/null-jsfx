/*
 * NULL JSFX - Ladder Filter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/ladder_filter.jsfx, computed in double like EEL2: a Moog-style
 * 4-pole zero-delay-feedback ladder (lowpass, or highpass from the stage mix) with a soft
 * saturator (tanh approximation) at the ladder input, Drive into it and Gain after it.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullladder {

static constexpr double kPi = 3.141592653589793;

struct Coeffs {
    double fc = 1000, wg = 0, G = 0, b = 0, Gb = 0, G2b = 0, G3b = 0, G4 = 0;
    double k = 0.5, drive = 1, comp = 1, outGain = 1;
    int mode = 0;
};

// the JSFX's @slider
inline Coeffs coeffs(double sr, double cutoff, double reso, double drivePct, int mode, double gainDb) noexcept
{
    Coeffs c;
    c.fc = std::min(cutoff, sr * 0.45);
    c.wg = std::tan(kPi * (c.fc / sr));
    c.G = c.wg / (1 + c.wg);
    c.b = 1 / (1 + c.wg);
    c.Gb = c.G * c.b; c.G2b = c.G * c.Gb; c.G3b = c.G * c.G2b;
    c.G4 = c.G * c.G * c.G * c.G;
    c.k = std::min(reso, 3.95);
    c.drive = 1 + drivePct / 100 * 4;
    c.mode = mode;
    c.comp = 1 + 0.5 * c.k;
    c.outGain = std::pow(10.0, gainDb / 20) / std::sqrt(c.drive);
    return c;
}

// display: the small-signal response in dB at f (lf_db in the GFX)
inline double responseDb(const Coeffs& c, double f, double sr) noexcept
{
    const double w = 2 * kPi * std::min(f, sr * 0.499) / sr;
    const double cw = std::cos(w), sw = std::sin(w), g = c.wg;
    const double nr = g * (1 + cw), ni = -g * sw;
    const double dr = (1 + g) - (1 - g) * cw, di = (1 - g) * sw;
    const double dd = dr * dr + di * di;
    const double hr = (nr * dr + ni * di) / dd, hi = (ni * dr - nr * di) / dd;
    double ar = hr * hr - hi * hi, ai = 2 * hr * hi;
    const double pr = ar * ar - ai * ai, pi = 2 * ar * ai;
    const double tr = 1 - hr, ti = -hi;
    ar = tr * tr - ti * ti; ai = 2 * tr * ti;
    const double er = ar * ar - ai * ai, ei = 2 * ar * ai;
    const double qr = c.mode == 0 ? pr : er, qi = c.mode == 0 ? pi : ei;
    const double mr = 1 + c.k * pr, mi = c.k * pi;
    const double gg = c.drive * c.outGain;
    return 10 * std::log10(std::max(c.comp * c.comp * (qr * qr + qi * qi) / (mr * mr + mi * mi) * gg * gg, 1e-10));
}

class LadderProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fL = State();
        fR = State();
    }

    // @slider
    void setParams(double cutoff, double reso, double drivePct, int mode, double gainDb)
    {
        fC = coeffs(fSr, cutoff, reso, drivePct, mode, gainDb);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            outL[n] = static_cast<T>(ladder(fL, double(inL[n]) * fC.drive) * fC.outGain);
            outR[n] = static_cast<T>(ladder(fR, double(inR[n]) * fC.drive) * fC.outGain);
        }
    }

private:
    struct State { double s1 = 0, s2 = 0, s3 = 0, s4 = 0; };

    double ladder(State& s, double x) const noexcept
    {
        const Coeffs& c = fC;
        const double S = c.G3b * s.s1 + c.G2b * s.s2 + c.Gb * s.s3 + c.b * s.s4;
        double u = (x * c.comp - c.k * S) / (1 + c.k * c.G4);
        // soft saturation inside the loop (tanh approximation)
        u = std::max(-3.0, std::min(3.0, u));
        u = u * (27 + u * u) / (27 + 9 * u * u);
        double v;
        v = (u - s.s1) * c.G;  const double y1 = v + s.s1; s.s1 = y1 + v;
        v = (y1 - s.s2) * c.G; const double y2 = v + s.s2; s.s2 = y2 + v;
        v = (y2 - s.s3) * c.G; const double y3 = v + s.s3; s.s3 = y3 + v;
        v = (y3 - s.s4) * c.G; const double y4 = v + s.s4; s.s4 = y4 + v;
        return c.mode == 0 ? y4 : u - 4 * y1 + 6 * y2 - 4 * y3 + y4;
    }

    double fSr = 48000.0;
    Coeffs fC;
    State fL, fR;
};

} // namespace nullladder
