/*
 * NULL JSFX - DeEsser, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/deesser.jsfx, computed in double like EEL2: a band-pass
 * (Q 2) around Target isolates the sibilance, an envelope follower (0.5 ms / 60 ms) measures it
 * and only that band is turned down, from -30 dB up to full Reduction 12 dB above. Mode Wide
 * turns the whole signal down instead; Threshold Mode Manual uses Threshold (dB) in place of the
 * fixed -30 dB; Range caps the reduction (only when it is above Reduction); Listen outputs only
 * what is removed. Suppress Freq (0 = Target), Suppress Q and Suppress Shape (Band or a
 * high-pass Shelf) give the band that is turned down a filter of its own; at their defaults
 * the measured band is the one turned down, as before.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nulldeesser {

struct Coefs {
    double b0, b2, a1, a2;
};

// a biquad with a b1 term: the suppress filter (band-pass or high-pass)
struct Coefs5 {
    double b0, b1, b2, a1, a2;
};

// the suppress filter of the JSFX's @slider (freq 0 = Target)
inline Coefs5 suppressFilter(double freq, double target, double q, bool shelf, double sr) noexcept
{
    const double w0 = 2.0 * 3.141592653589793 * std::min(freq > 0.0 ? freq : target, sr * 0.45) / sr;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    Coefs5 c;
    if (shelf)
    {
        c.b0 = (1.0 + std::cos(w0)) / 2.0 / a0;
        c.b1 = -(1.0 + std::cos(w0)) / a0;
        c.b2 = c.b0;
    }
    else
    {
        c.b0 = alpha / a0;
        c.b1 = 0.0;
        c.b2 = -alpha / a0;
    }
    c.a1 = -2.0 * std::cos(w0) / a0;
    c.a2 = (1.0 - alpha) / a0;
    return c;
}

// whether the suppress sliders differ from their defaults (EEL2's fuzzy != for Q)
inline bool suppressOwn(double freq, double q, bool shelf) noexcept
{
    return freq > 0.0 || std::fabs(q - 2.0) >= 0.00001 || shelf;
}

// response in dB at f of x - cut * filter(x) for the suppress filter (display)
inline double responseDb5(const Coefs5& c, double f, double sr, double cut) noexcept
{
    const double w = 2.0 * 3.141592653589793 * f / sr;
    const double nr = c.b0 + c.b1 * std::cos(w) + c.b2 * std::cos(2.0 * w);
    const double ni = -(c.b1 * std::sin(w) + c.b2 * std::sin(2.0 * w));
    const double dr = 1.0 + c.a1 * std::cos(w) + c.a2 * std::cos(2.0 * w);
    const double di = -(c.a1 * std::sin(w) + c.a2 * std::sin(2.0 * w));
    const double dd = dr * dr + di * di;
    const double br = (nr * dr + ni * di) / dd, bi = (ni * dr - nr * di) / dd;
    const double hr = 1.0 - cut * br, hi = -cut * bi;
    return 10.0 * std::log10(std::max(hr * hr + hi * hi, 0.0000000001));
}

// the band-pass of the JSFX's @slider
inline Coefs bandpass(double freq, double sr) noexcept
{
    const double w0 = 2.0 * 3.141592653589793 * std::min(freq, sr * 0.45) / sr;
    const double q = 2.0;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    return { alpha / a0, -alpha / a0, -2.0 * std::cos(w0) / a0, (1.0 - alpha) / a0 };
}

// response in dB at f of x - cut * bandpass(x) (for the display)
inline double responseDb(const Coefs& c, double f, double sr, double cut) noexcept
{
    const double w = 2.0 * 3.141592653589793 * f / sr;
    const double nr = c.b0 + c.b2 * std::cos(2.0 * w), ni = -c.b2 * std::sin(2.0 * w);
    const double dr = 1.0 + c.a1 * std::cos(w) + c.a2 * std::cos(2.0 * w);
    const double di = -(c.a1 * std::sin(w) + c.a2 * std::sin(2.0 * w));
    const double dd = dr * dr + di * di;
    const double br = (nr * dr + ni * di) / dd, bi = (ni * dr - nr * di) / dd;
    const double hr = 1.0 - cut * br, hi = -cut * bi;
    return 10.0 * std::log10(std::max(hr * hr + hi * hi, 0.0000000001));
}

class DeEsserProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fThreshold = std::pow(10.0, -30.0 / 20.0);
        fThrAuto = fThreshold;
        fAtt = std::exp(-1.0 / (0.0005 * sr));
        fRel = std::exp(-1.0 / (0.06 * sr));
        fEnv = 0.0;
        fX1l = fX2l = fY1l = fY2l = 0.0;
        fX1r = fX2r = fY1r = fY2r = 0.0;
        fSx1l = fSx2l = fSy1l = fSy2l = 0.0;
        fSx1r = fSx2r = fSy1r = fSy2r = 0.0;
        fCut = 0.0;
    }

    // @slider
    void setParams(double freq, double reductionDb, bool wide = false, bool manual = false, double thresholdDb = -30.0,
                   double rangeDb = -18.0, bool listen = false, double suppressHz = 0.0, double suppressQ = 2.0,
                   bool shelf = false)
    {
        fC = bandpass(freq, fSr);
        fMaxCut = 1.0 - std::pow(10.0, reductionDb / 20.0);
        fWide = wide;
        fThreshold = manual ? std::pow(10.0, thresholdDb / 20.0) : fThrAuto;
        fRangeOn = rangeDb > reductionDb; // a Range below Reduction never limits
        fRangeCut = 1.0 - std::pow(10.0, rangeDb / 20.0);
        fListen = listen;
        fOwn = suppressOwn(suppressHz, suppressQ, shelf);
        if (fOwn)
            fS = suppressFilter(suppressHz, freq, suppressQ, shelf, fSr);
    }

    // the cut the band (or, Wide, the signal) gets at most: Reduction, capped by Range
    double maxCut() const noexcept { return fRangeOn ? std::min(fMaxCut, fRangeCut) : fMaxCut; }
    bool wide() const noexcept { return fWide; }
    double threshold() const noexcept { return fThreshold; }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        const double bl = fC.b0 * l + fC.b2 * fX2l - fC.a1 * fY1l - fC.a2 * fY2l;
        fX2l = fX1l; fX1l = l; fY2l = fY1l; fY1l = bl;
        const double br = fC.b0 * r + fC.b2 * fX2r - fC.a1 * fY1r - fC.a2 * fY2r;
        fX2r = fX1r; fX1r = r; fY2r = fY1r; fY1r = br;
        double sl = 0.0, sr = 0.0;
        if (fOwn)
        {
            sl = fS.b0 * l + fS.b1 * fSx1l + fS.b2 * fSx2l - fS.a1 * fSy1l - fS.a2 * fSy2l;
            fSx2l = fSx1l; fSx1l = l; fSy2l = fSy1l; fSy1l = sl;
            sr = fS.b0 * r + fS.b1 * fSx1r + fS.b2 * fSx2r - fS.a1 * fSy1r - fS.a2 * fSy2r;
            fSx2r = fSx1r; fSx1r = r; fSy2r = fSy1r; fSy1r = sr;
        }

        const double level = std::max(std::fabs(bl), std::fabs(br));
        fEnv = level > fEnv ? fAtt * fEnv + (1.0 - fAtt) * level : fRel * fEnv + (1.0 - fRel) * level;
        const double over = fEnv > fThreshold ? std::min(std::log(fEnv / fThreshold) / std::log(4.0), 1.0) : 0.0;
        fCut = over * fMaxCut;
        if (fRangeOn)
            fCut = std::min(fCut, fRangeCut);
        double rl, rr;
        if (fWide)
        {
            rl = l * fCut;
            rr = r * fCut;
        }
        else if (fOwn)
        {
            rl = sl * fCut;
            rr = sr * fCut;
        }
        else
        {
            rl = bl * fCut;
            rr = br * fCut;
        }
        if (fListen)
        {
            l = rl;
            r = rr;
        }
        else
        {
            l -= rl;
            r -= rr;
        }
    }

    double envelope() const noexcept { return fEnv; }
    bool own() const noexcept { return fOwn; }
    const Coefs5& suppress() const noexcept { return fS; }
    double cut() const noexcept { return fCut; }

private:
    bool fWide = false, fRangeOn = false, fListen = false, fOwn = false;
    Coefs5 fS { 0.0, 0.0, 0.0, 0.0, 0.0 };
    double fSx1l = 0, fSx2l = 0, fSy1l = 0, fSy2l = 0, fSx1r = 0, fSx2r = 0, fSy1r = 0, fSy2r = 0;
    double fThrAuto = 0.0, fRangeCut = 0.0;
    double fSr = 48000.0, fThreshold = 0.0, fAtt = 0.0, fRel = 0.0, fEnv = 0.0, fMaxCut = 0.0, fCut = 0.0;
    Coefs fC { 0.0, 0.0, 0.0, 0.0 };
    double fX1l = 0, fX2l = 0, fY1l = 0, fY2l = 0, fX1r = 0, fX2r = 0, fY1r = 0, fY2r = 0;
};

} // namespace nulldeesser
