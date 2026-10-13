/*
 * NULL JSFX - Resonator, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/resonator.jsfx, computed in double like EEL2: up to four
 * Karplus-Strong string loops per channel (one per chord note), each a delay line, a first-order
 * allpass for the fractional period and a symmetric 3-tap low-pass (exactly one sample of delay),
 * excited by the 20 Hz high-passed input scaled by sqrt(1 - g^2). The wet sum passes a DC blocker
 * and a soft ceiling, then mixes with the dry signal.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullres {

static constexpr double kPi = 3.141592653589793;
static constexpr double kTiny = 1e-21;

// chord intervals in semitones (-1 = unused), as the JSFX's table
static constexpr int kChords[8][4] = {
    { 0, -1, -1, -1 }, // Unison
    { 0, 12, 24, -1 }, // Octaves
    { 0, 7, 12, -1 },  // Fifth
    { 0, 4, 7, 12 },   // Major
    { 0, 3, 7, 12 },   // Minor
    { 0, 5, 7, 12 },   // Sus4
    { 0, 4, 7, 11 },   // Maj7
    { 0, 3, 7, 10 },   // Min7
};

// one string loop: v * 2 + c (voice, channel)
struct Loop {
    double m = 1.0, ap = 0.0, g = 0.0, k = 0.0, len = 2.0; // delay, allpass coef, gain, input scale, period
    int note = 0;
};

// the JSFX's @slider; s[0..6] = slider1..slider7
struct Params {
    Loop loops[8];
    int nv = 1;
    double lpA0 = 0.0, lpA1 = 1.0, hpC = 0.0, dcC = 0.0, wetGain = 1.0, mix = 0.5;
};

inline Params computeParams(double sr, const double* s) noexcept
{
    Params p;
    const double root = s[0], octave = s[1], decay = std::max(0.1, s[3]);
    const int chord = std::max(0, std::min(7, int(s[2])));
    const double bright = s[4] / 100.0, spread = s[5] / 100.0;
    p.mix = s[6] / 100.0;
    p.lpA0 = 0.25 * (1.0 - bright) * (1.0 - bright);
    p.lpA1 = 1.0 - 2.0 * p.lpA0;
    p.hpC = std::exp(-2.0 * kPi * 20.0 / sr);
    p.dcC = std::exp(-2.0 * kPi * 8.0 / sr);
    int nv = 0;
    for (int v = 0; v < 4; ++v)
    {
        if (kChords[chord][v] < 0)
            continue;
        const double note = 12.0 * (octave + 1.0) + root + kChords[chord][v];
        for (int c = 0; c < 2; ++c)
        {
            const double f0 = 440.0 * std::pow(2.0, (note - 69.0 + (c == 0 ? -1.0 : 1.0) * spread * 0.15) / 12.0);
            const double len = sr / f0;
            const double m = std::floor(len - 1.5);
            const double d = len - 1.0 - m;
            Loop& l = p.loops[nv * 2 + c];
            l.m = m;
            l.ap = (1.0 - d) / (1.0 + d);
            l.g = std::min(0.99995, std::pow(10.0, -3.0 / (f0 * decay)));
            l.k = std::sqrt(1.0 - l.g * l.g);
            l.len = len;
            l.note = int(note);
        }
        ++nv;
    }
    p.nv = nv;
    p.wetGain = 1.0 / std::sqrt(double(nv));
    return p;
}

// display: the small-signal wet gain in dB at f (each loop as a comb of period len with the
// low-pass, power-summed over voices and both channels), as res_db in the GFX
inline double responseDb(const Params& p, double f, double sr) noexcept
{
    const double w = 2.0 * kPi * f / sr;
    const double h = p.lpA1 + 2.0 * p.lpA0 * std::cos(w);
    double sum = 0.0;
    for (int i = 0; i < p.nv * 2; ++i)
    {
        const Loop& l = p.loops[i];
        const double gg = l.g * h;
        sum += l.k * l.k / (1.0 - 2.0 * gg * std::cos(w * l.len) + gg * gg);
    }
    return 10.0 * std::log10(std::max(sum * p.wetGain * p.wetGain * 0.5, 1e-10));
}

// display: the peak of harmonic n of loop i in dB (same power sum as responseDb, one loop)
inline double peakDb(const Params& p, int i, int n) noexcept
{
    const Loop& l = p.loops[i];
    const double w = 2.0 * kPi * n / l.len;
    const double gg = l.g * (p.lpA1 + 2.0 * p.lpA0 * std::cos(w));
    return 10.0 * std::log10(std::max(l.k * l.k / ((1.0 - gg) * (1.0 - gg)) * p.wetGain * p.wetGain * 0.5, 1e-10));
}

class ResonatorProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr / 30.0)) + 8;
        fBuf.assign(size_t(8 * fSize), 0.0);
        for (auto& s : fSt)
            s = State();
        fW = 0;
        fHpXL = fHpYL = fHpXR = fHpYR = 0.0;
        fDcXL = fDcYL = fDcXR = fDcYR = 0.0;
    }

    // @slider
    void setParams(const double* s) { fP = computeParams(fSr, s); }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        const double mix = fP.mix;
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            fHpYL = fP.hpC * (fHpYL + xl - fHpXL); fHpXL = xl;
            fHpYR = fP.hpC * (fHpYR + xr - fHpXR); fHpXR = xr;
            if (std::fabs(fHpYL) < kTiny) fHpYL = 0.0;
            if (std::fabs(fHpYR) < kTiny) fHpYR = 0.0;

            double wl = 0.0, wr = 0.0;
            for (int v = 0; v < fP.nv; ++v)
            {
                wl += tick(fHpYL, v * 2);
                wr += tick(fHpYR, v * 2 + 1);
            }
            if (++fW >= fSize)
                fW = 0;
            wl *= fP.wetGain;
            wr *= fP.wetGain;

            fDcYL = fP.dcC * (fDcYL + wl - fDcXL); fDcXL = wl;
            fDcYR = fP.dcC * (fDcYR + wr - fDcXR); fDcXR = wr;
            if (std::fabs(fDcYL) < kTiny) fDcYL = 0.0;
            if (std::fabs(fDcYR) < kTiny) fDcYR = 0.0;

            wl = ceiling(fDcYL);
            wr = ceiling(fDcYR);
            outL[n] = static_cast<T>(xl * (1.0 - mix) + wl * mix);
            outR[n] = static_cast<T>(xr * (1.0 - mix) + wr * mix);
        }
    }

private:
    struct State { double apX = 0.0, apY = 0.0, f1 = 0.0, f2 = 0.0; };

    // soft ceiling: linear up to 0.7, then bends smoothly towards 1.0
    static double ceiling(double x)
    {
        const double a = std::fabs(x);
        if (a > 0.7)
        {
            const double u = (a - 0.7) / 0.3;
            return (x > 0.0 ? 1.0 : -1.0) * (0.7 + 0.3 * u / (1.0 + u));
        }
        return x;
    }

    // string_tick in the JSFX
    double tick(double x, int i)
    {
        const Loop& l = fP.loops[i];
        State& s = fSt[i];
        double* b = fBuf.data() + size_t(i) * size_t(fSize);
        double rp = double(fW) - l.m;
        if (rp < 0.0)
            rp += fSize;
        const double in = b[size_t(rp)];
        const double a = l.ap * (in - s.apY) + s.apX;
        s.apX = in;
        s.apY = a;
        const double f = fP.lpA0 * (a + s.f2) + fP.lpA1 * s.f1;
        s.f2 = s.f1;
        s.f1 = a;
        double y = x * l.k + l.g * f;
        if (std::fabs(y) < kTiny)
            y = 0.0;
        b[fW] = y;
        return y;
    }

    double fSr = 48000.0;
    std::vector<double> fBuf;
    State fSt[8];
    int fSize = 1, fW = 0;
    Params fP;
    double fHpXL = 0, fHpYL = 0, fHpXR = 0, fHpYR = 0;
    double fDcXL = 0, fDcYL = 0, fDcXR = 0, fDcYR = 0;
};

} // namespace nullres
