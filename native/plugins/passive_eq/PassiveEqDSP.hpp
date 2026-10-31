/*
 * NULL JSFX - Passive EQ, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/passive_eq.jsfx, computed in double in the EEL2 order: a low
 * shelf boost at 1.2 x Low Freq, a gentler low shelf cut at 2.2 x Low Freq, a peaking boost at
 * High Freq (width from Bandwidth) and a high shelf cut at Atten Freq (RBJ biquads, each skipped
 * at 0), then the Tube stage (asymmetric tanh with a 10 Hz DC blocker, skipped at 0 %) and Output.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullpassiveeq {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    int lowFreq = 2, highFreq = 3, attenFreq = 1;
    double lowBoost = 0.0, lowAtten = 0.0, highBoost = 0.0, bandwidth = 5.0, highAtten = 0.0, tube = 0.0, output = 0.0;
};

struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// pe_tanh of the JSFX: (e^2x - 1) / (e^2x + 1), clamped
inline double eelTanh(double x) noexcept
{
    if (x > 20.0)
        return 1.0;
    if (x < -20.0)
        return -1.0;
    const double e = std::exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}

// pe_bq: kind 0 = low shelf (slope S), 1 = high shelf (slope S), 2 = peaking (Q)
inline Biquad biquad(int kind, double f, double sq, double g, double sr) noexcept
{
    Biquad q;
    const double w0 = 2.0 * kPi * std::fmin(f, 0.45 * sr) / sr;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double A = std::pow(10.0, g / 40.0);
    if (kind == 2)
    {
        const double al = sw / (2.0 * sq);
        const double a0 = 1.0 + al / A;
        q.b0 = (1.0 + al * A) / a0;
        q.b1 = -2.0 * cw / a0;
        q.b2 = (1.0 - al * A) / a0;
        q.a1 = q.b1;
        q.a2 = (1.0 - al / A) / a0;
        return q;
    }
    const double al = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / sq - 1.0) + 2.0);
    const double b2a = 2.0 * std::sqrt(A) * al;
    if (kind == 0)
    {
        const double a0 = (A + 1.0) + (A - 1.0) * cw + b2a;
        q.b0 = A * ((A + 1.0) - (A - 1.0) * cw + b2a) / a0;
        q.b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0;
        q.b2 = A * ((A + 1.0) - (A - 1.0) * cw - b2a) / a0;
        q.a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0;
        q.a2 = ((A + 1.0) + (A - 1.0) * cw - b2a) / a0;
    }
    else
    {
        const double a0 = (A + 1.0) - (A - 1.0) * cw + b2a;
        q.b0 = A * ((A + 1.0) + (A - 1.0) * cw + b2a) / a0;
        q.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
        q.b2 = A * ((A + 1.0) + (A - 1.0) * cw - b2a) / a0;
        q.a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
        q.a2 = ((A + 1.0) - (A - 1.0) * cw - b2a) / a0;
    }
    return q;
}

// |H|^2 of one biquad at angular frequency w
inline double biquadPower(const Biquad& q, double w) noexcept
{
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
    const double nr = q.b0 + q.b1 * c1 + q.b2 * c2, ni = -(q.b1 * s1 + q.b2 * s2);
    const double dr = 1.0 + q.a1 * c1 + q.a2 * c2, di = -(q.a1 * s1 + q.a2 * s2);
    return (nr * nr + ni * ni) / (dr * dr + di * di);
}

struct Coeffs {
    Biquad lb, la, hb, ha;
    bool lbOn = false, laOn = false, hbOn = false, haOn = false;
    double tube = 0.0, tk = 1.0, tb = 0.0, tz = 0.0, dca = 0.0, og = 1.0;
};

inline double lowFreqHz(int i) noexcept { return i == 0 ? 20.0 : i == 1 ? 30.0 : i == 2 ? 60.0 : 100.0; }
inline double highFreqHz(int i) noexcept
{
    return i == 0 ? 3000.0 : i == 1 ? 4000.0 : i == 2 ? 5000.0 : i == 3 ? 8000.0 : i == 4 ? 10000.0 : i == 5 ? 12000.0 : 16000.0;
}
inline double attenFreqHz(int i) noexcept { return i == 0 ? 5000.0 : i == 1 ? 10000.0 : 20000.0; }

// the @slider code of passive_eq.jsfx
inline Coeffs coeffsFor(const Params& p, double sr) noexcept
{
    Coeffs c;
    const double lf = lowFreqHz(p.lowFreq), hf = highFreqHz(p.highFreq), af = attenFreqHz(p.attenFreq);
    c.lbOn = p.lowBoost > 0.0;
    c.laOn = p.lowAtten > 0.0;
    c.hbOn = p.highBoost > 0.0;
    c.haOn = p.highAtten > 0.0;
    c.lb = biquad(0, 1.2 * lf, 0.7, 1.35 * p.lowBoost, sr);
    c.la = biquad(0, 2.2 * lf, 0.4, -1.75 * p.lowAtten, sr);
    c.hb = biquad(2, hf, 2.2 - 0.19 * p.bandwidth, 1.8 * p.highBoost, sr);
    c.ha = biquad(1, af, 0.6, -2.0 * p.highAtten, sr);
    c.tube = p.tube / 100.0;
    c.tk = 1.0 + 3.0 * c.tube;
    c.tb = 0.2 * c.tube;
    c.tz = eelTanh(c.tb);
    c.dca = std::exp(-2.0 * kPi * 10.0 / sr);
    c.og = std::pow(10.0, p.output / 20.0);
    return c;
}

// the EQ's response in dB at f (Output included, Tube left out), for the display
inline double magnitudeDb(const Coeffs& c, double f, double sr) noexcept
{
    const double w = 2.0 * kPi * std::fmin(f, sr * 0.499) / sr;
    double p = c.og * c.og;
    if (c.lbOn) p *= biquadPower(c.lb, w);
    if (c.laOn) p *= biquadPower(c.la, w);
    if (c.hbOn) p *= biquadPower(c.hb, w);
    if (c.haOn) p *= biquadPower(c.ha, w);
    return 10.0 * std::log10(std::fmax(p, 1e-10));
}

class PassiveEqProcessor {
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
        fDcl = fDcr = fDxl = fDxr = 0.0;
    }

    const Coeffs& coeffs() const noexcept { return fC; }

    void processSample(double& l, double& r) noexcept
    {
        const Coeffs& c = fC;
        l = chain(l, 0);
        r = chain(r, 1);
        if (c.tube > 0.0)
        {
            l = (eelTanh(c.tk * l + c.tb) - c.tz) / c.tk;
            r = (eelTanh(c.tk * r + c.tb) - c.tz) / c.tk;
            const double yl = l - fDxl + c.dca * fDcl;
            fDxl = l;
            fDcl = yl;
            l = yl;
            const double yr = r - fDxr + c.dca * fDcr;
            fDxr = r;
            fDcr = yr;
            r = yr;
        }
        l = l * c.og;
        r = r * c.og;
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
        if (c.lbOn) x = run(c.lb, x, ch * 8);
        if (c.laOn) x = run(c.la, x, ch * 8 + 2);
        if (c.hbOn) x = run(c.hb, x, ch * 8 + 4);
        if (c.haOn) x = run(c.ha, x, ch * 8 + 6);
        return x;
    }

    double fSr = 48000.0;
    Params fP;
    Coeffs fC = coeffsFor(Params(), 48000.0);
    double fSt[16] = {};
    double fDcl = 0.0, fDcr = 0.0, fDxl = 0.0, fDxr = 0.0;
};

} // namespace nullpassiveeq
