/*
 * NULL JSFX - Speaker Sim, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/speaker_sim.jsfx, computed in double in the EEL2 order: per
 * device a 24 dB/oct Butterworth high-pass (two RBJ biquads), two peaking bands, a soft clip
 * tanh(g x) / g when there is drive, and a 24 dB/oct low-pass; the mono devices sum the two
 * channels first. Then the dry / wet Mix and Output.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullspeakersim {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kDevices = 8;

// the DEV table of the JSFX: mono, hp, lp, f1, g1, q1, f2, g2, q2, drive
struct Device {
    double mono, hp, lp, f1, g1, q1, f2, g2, q2, drive;
};
static const Device kDevice[kDevices] = {
    { 1, 450, 9000, 1200, 4, 1.2, 3500, 5, 1.5, 0 },    // Phone
    { 0, 220, 14000, 800, 3, 1, 4000, 4, 2, 0 },        // Laptop
    { 0, 70, 15000, 2800, 5, 1.4, 7000, -3, 2, 0 },     // Earbuds
    { 0, 130, 12000, 350, 2, 1, 2500, 3, 1.2, 0 },      // TV
    { 1, 180, 5500, 1000, 4, 0.9, 2500, 2, 1, 0 },      // Radio
    { 1, 320, 3400, 1500, 5, 1, 2800, 3, 1.5, 0 },      // Telephone
    { 1, 550, 4200, 1800, 9, 1.3, 3000, 4, 2, 0.5 },    // Megaphone
    { 0, 40, 16000, 65, 6, 0.9, 3000, -3, 1, 0 },       // Car
};

struct Params {
    int device = 0;
    double mix = 100.0, drive = 0.0, output = 0.0;
};

struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// ss_tanh of the JSFX: (e^2x - 1) / (e^2x + 1), clamped
inline double eelTanh(double x) noexcept
{
    if (x > 20.0)
        return 1.0;
    if (x < -20.0)
        return -1.0;
    const double e = std::exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}

// ss_bq: kind 0 = high-pass, 1 = low-pass (Q q), 2 = peaking (gain g dB, Q q)
inline Biquad biquad(int kind, double f, double q, double g, double sr) noexcept
{
    Biquad b;
    const double w0 = 2.0 * kPi * std::fmin(f, 0.45 * sr) / sr;
    const double cw = std::cos(w0);
    const double al = std::sin(w0) / (2.0 * q);
    if (kind == 2)
    {
        const double A = std::pow(10.0, g / 40.0);
        const double a0 = 1.0 + al / A;
        b.b0 = (1.0 + al * A) / a0;
        b.b1 = -2.0 * cw / a0;
        b.b2 = (1.0 - al * A) / a0;
        b.a1 = b.b1;
        b.a2 = (1.0 - al / A) / a0;
    }
    else
    {
        const double a0 = 1.0 + al;
        b.b0 = (kind ? (1.0 - cw) / 2.0 : (1.0 + cw) / 2.0) / a0;
        b.b1 = (kind ? 1.0 - cw : -(1.0 + cw)) / a0;
        b.b2 = b.b0;
        b.a1 = -2.0 * cw / a0;
        b.a2 = (1.0 - al) / a0;
    }
    return b;
}

inline double biquadPower(const Biquad& q, double w) noexcept
{
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    const double nr = q.b0 + q.b1 * c1 + q.b2 * c2, ni = -(q.b1 * s1 + q.b2 * s2);
    const double dr = 1.0 + q.a1 * c1 + q.a2 * c2, di = -(q.a1 * s1 + q.a2 * s2);
    return (nr * nr + ni * ni) / (dr * dr + di * di);
}

struct Coeffs {
    Biquad f[6];
    bool mono = true;
    double drv = 0.0, dg = 1.0, mix = 1.0, og = 1.0;
};

// the @slider code of speaker_sim.jsfx
inline Coeffs coeffsFor(const Params& p, double sr) noexcept
{
    Coeffs c;
    const Device& d = kDevice[p.device < 0 ? 0 : p.device >= kDevices ? kDevices - 1 : p.device];
    c.mono = d.mono != 0.0;
    c.f[0] = biquad(0, d.hp, 0.5411961001461969, 0.0, sr);
    c.f[1] = biquad(0, d.hp, 1.3065629648763766, 0.0, sr);
    c.f[2] = biquad(2, d.f1, d.q1, d.g1, sr);
    c.f[3] = biquad(2, d.f2, d.q2, d.g2, sr);
    c.f[4] = biquad(1, d.lp, 0.5411961001461969, 0.0, sr);
    c.f[5] = biquad(1, d.lp, 1.3065629648763766, 0.0, sr);
    c.drv = d.drive + p.drive / 100.0;
    c.dg = 1.0 + 8.0 * c.drv;
    c.mix = p.mix / 100.0;
    c.og = std::pow(10.0, p.output / 20.0);
    return c;
}

// the device's filters in dB at f (clip, Mix and Output left out), for the display
inline double magnitudeDb(const Coeffs& c, double f, double sr) noexcept
{
    const double w = 2.0 * kPi * std::fmin(f, sr * 0.499) / sr;
    double p = 1.0;
    for (const Biquad& b : c.f)
        p *= biquadPower(b, w);
    return 10.0 * std::log10(std::fmax(p, 1e-10));
}

class SpeakerSimProcessor {
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

    void reset() noexcept
    {
        for (double& s : fSt)
            s = 0.0;
    }

    void processSample(double& l, double& r) noexcept
    {
        const Coeffs& c = fC;
        double wl, wr;
        if (c.mono)
        {
            const double m = (l + r) * 0.5;
            wl = wr = chain(m, 0);
        }
        else
        {
            wl = chain(l, 0);
            wr = chain(r, 1);
        }
        l = (l + (wl - l) * c.mix) * c.og;
        r = (r + (wr - r) * c.mix) * c.og;
    }

private:
    double run(const Biquad& q, double x, int s) noexcept
    {
        const double y = q.b0 * x + fSt[s];
        fSt[s] = q.b1 * x - q.a1 * y + fSt[s + 1];
        fSt[s + 1] = q.b2 * x - q.a2 * y;
        return y;
    }

    double chain(double x, int ch) noexcept
    {
        const Coeffs& c = fC;
        x = run(c.f[0], x, ch * 12);
        x = run(c.f[1], x, ch * 12 + 2);
        x = run(c.f[2], x, ch * 12 + 4);
        x = run(c.f[3], x, ch * 12 + 6);
        if (c.drv > 0.0)
            x = eelTanh(c.dg * x) / c.dg;
        x = run(c.f[4], x, ch * 12 + 8);
        return run(c.f[5], x, ch * 12 + 10);
    }

    double fSr = 48000.0;
    Params fP;
    Coeffs fC = coeffsFor(Params(), 48000.0);
    double fSt[24] = {};
};

} // namespace nullspeakersim
