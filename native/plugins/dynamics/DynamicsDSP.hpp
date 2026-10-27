/*
 * NULL JSFX - Dynamics, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/dynamics.jsfx, computed in double like EEL2: a stereo-linked
 * compressor (above Threshold) or downward expander (below it) with Ratio, Knee, Attack,
 * Release and Makeup, four detector styles (Clean, Bus, FET, Opto), a high-pass on the detector,
 * a dry/wet Mix for parallel compression and an external sidechain (JSFX pins 3/4).
 * ln10 is the JSFX's truncated 2.302585, kept for an exact match. Every operation follows the
 * JSFX in the same order, so at the defaults (Clean, HPF 20 Hz, Mix 100 %, Internal) the result
 * is the original plugin's.
 * Upward (Style 4, appended in 2.4): an RMS detector (10 ms) and a lift instead of a reduction:
 * compressing, below Threshold (upward compression), expanding, above it (upward expansion); at
 * most 24 dB, and compressing it fades out from 48 to 72 dB under Threshold.
 * Appended in 2.7: Channels (Mid / Side: only that half of the mid/side pair is processed, on
 * both channels, and the other half is added back after) and Listen Delta (the input minus the
 * processed signal before Makeup and Mix). At Stereo and Output nothing changes.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nulldynamics {

static constexpr double kLn10 = 2.302585;
static constexpr double kDbToLin = kLn10 / 20.0;
static constexpr double kPi = 3.141592653589793;

enum Style { kClean = 0, kBus = 1, kFet = 2, kOpto = 3, kUpward = 4 };

struct Params {
    bool expander = false;
    double thresholdDb = -20.0, ratio = 4.0, kneeDb = 6.0, makeupDb = 0.0;
    int style = kClean;
    // the style's gain computer (set by computeStyle): slope (1 - 1/ratio) and knee
    double slope = 0.75, knee = 6.0;
};

// @slider: the slope and knee the style uses (Bus: extra knee and, compressing, a slope that
// makes up for the half feedback so the steady state sits on the plain curve; FET: half the knee;
// Opto: at least 10 dB of knee)
inline void computeStyle(Params& p) noexcept
{
    p.slope = 1.0 - 1.0 / p.ratio;
    p.knee = p.kneeDb;
    if (p.style == kBus)
    {
        p.knee = p.kneeDb + 4.0;
        if (!p.expander)
            p.slope = p.slope / (1.0 - 0.5 * p.slope);
    }
    if (p.style == kFet)
        p.knee = p.kneeDb * 0.5;
    if (p.style == kOpto)
        p.knee = std::max(p.kneeDb, 10.0);
}

// Upward: the lift in dB (positive) for a detector level in dB, from the style's slope and knee
inline double liftDb(const Params& p, double inputDb) noexcept
{
    const double x = !p.expander ? p.thresholdDb - inputDb : inputDb - p.thresholdDb;
    double t = 0.0;
    if (x > 0.0)
    {
        if (p.knee > 0.0 && x < p.knee)
            t = p.slope * x * x / (2.0 * p.knee);
        else
            t = p.slope * (x - p.knee / 2.0);
        t = std::min(t, 24.0);
        if (!p.expander && x > 48.0)
            t *= std::max(0.0, 1.0 - (x - 48.0) / 24.0);
    }
    return t;
}

// gain reduction in dB for a detector level in dB (the JSFX's gain computer; Upward: minus the lift)
inline double grDb(const Params& p, double inputDb) noexcept
{
    if (p.style == kUpward)
        return -liftDb(p, inputDb);
    const double x = p.expander ? p.thresholdDb - inputDb : inputDb - p.thresholdDb;
    if (x <= 0.0)
        return 0.0;
    // quadratic soft knee: the slope rises smoothly from 1 to 1/ratio
    if (p.knee > 0.0 && x < p.knee)
        return p.slope * x * x / (2.0 * p.knee);
    return p.slope * (x - p.knee / 2.0);
}

// static curve: output level in dB for an input level d (for the display). For Bus compressing,
// the plain (1 - 1/ratio) slope: the feedback's steady state above the knee.
inline double curveDb(const Params& p, double d) noexcept
{
    Params q = p;
    q.slope = 1.0 - 1.0 / p.ratio;
    return d - grDb(q, d) + p.makeupDb;
}

class DynamicsProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fGainEnv = 1.0;
        fInputAbs = 0.0;
        fGr = fGrF = fGrS = fGrO1 = fGrO2 = fGrMem = 0.0;
        fDetMs = 0.0;
        fHpL1 = fHpL2 = fHpR1 = fHpR2 = 0.0;
    }

    // @slider
    void setParams(int mode, double thresholdDb, double ratio, double kneeDb, double attackMs, double releaseMs,
                   double makeupDb, int style = kClean, double hpfHz = 20.0, double mixPct = 100.0, bool external = false,
                   int channels = 0, bool delta = false)
    {
        fMs = channels;
        fDelta = delta;
        fP.expander = std::fabs(double(mode)) >= 0.00001; // EEL2: mode == 0 is fuzzy
        fP.thresholdDb = thresholdDb;
        fP.ratio = ratio;
        fP.kneeDb = kneeDb;
        fP.makeupDb = makeupDb;
        fP.style = style;
        fHpfOn = hpfHz > 20.0;
        fMixOn = mixPct < 100.0;
        fMix = mixPct / 100.0;
        fExternal = external;
        fAttack = attackMs > 0.0 ? 1.0 - std::exp(-1.0 / (attackMs * fSr / 1000.0)) : 1.0;
        fRelease = releaseMs > 0.0 ? 1.0 - std::exp(-1.0 / (releaseMs * fSr / 1000.0)) : 1.0;
        computeStyle(fP);
        fFetAtt = std::min(1.0, fAttack * 4.0);
        fOptoAtt = fAttack * 0.5;
        fRmsTc = 1.0 - std::exp(-1.0 / ((style == kOpto || style == kUpward ? 10.0 : 5.0) * fSr / 1000.0));
        fBusSatt = 1.0 - std::exp(-1.0 / (250.0 * fSr / 1000.0));
        fBusSrel = 1.0 - std::exp(-1.0 / ((200.0 + 4.0 * releaseMs) * fSr / 1000.0));
        fOptoSrel = fRelease * 0.1;
        fMemTc = 1.0 - std::exp(-1.0 / fSr);
        if (fHpfOn)
        {
            // detector high-pass: 12 dB/oct (RBJ biquad, Q = 0.7071)
            const double w = 2.0 * kPi * hpfHz / fSr;
            const double cs = std::cos(w);
            const double al = std::sin(w) / (2.0 * std::sqrt(0.5));
            const double a0 = 1.0 + al;
            fB0 = (1.0 + cs) / 2.0 / a0;
            fB1 = -(1.0 + cs) / a0;
            fA1 = -2.0 * cs / a0;
            fA2 = (1.0 - al) / a0;
        }
    }

    // @sample: l/r the track (changed in place), scL/scR the sidechain (JSFX pins 3/4)
    void processSample(double& l, double& r, double scL = 0.0, double scR = 0.0) noexcept
    {
        double other = 0.0;
        if (fMs != 0)
        {
            const double m = (l + r) * 0.5;
            const double s = (l - r) * 0.5;
            if (fMs == 1)
            {
                l = r = m;
                other = s;
            }
            else
            {
                l = r = s;
                other = m;
            }
        }
        double detL = fExternal ? scL : l, detR = fExternal ? scR : r;
        if (fHpfOn)
        {
            double y = fB0 * detL + fHpL1;
            fHpL1 = fB1 * detL - fA1 * y + fHpL2;
            fHpL2 = fB0 * detL - fA2 * y;
            detL = y;
            y = fB0 * detR + fHpR1;
            fHpR1 = fB1 * detR - fA1 * y + fHpR2;
            fHpR2 = fB0 * detR - fA2 * y;
            detR = y;
        }
        double in = std::max(std::fabs(detL), std::fabs(detR));
        if (in < 0.000001)
            in = 0.000001;
        fInputAbs = in;
        const int style = fP.style;
        double inputDb;
        if (style == kBus || style == kOpto || style == kUpward)
        {
            // crest-corrected RMS (a sine reads its peak)
            fDetMs = fDetMs + (in * in - fDetMs) * fRmsTc;
            double lv = std::sqrt(2.0 * fDetMs);
            if (lv < 0.000001)
                lv = 0.000001;
            inputDb = std::log(lv) / kLn10 * 20.0;
            if (style == kBus && !fP.expander)
                inputDb = inputDb - 0.5 * fGr;
        }
        else
            inputDb = std::log(in) / kLn10 * 20.0;
        const double t = grDb(fP, inputDb);

        if (style == kClean)
        {
            const double target = std::exp(-t * kDbToLin);
            if (target < fGainEnv)
                fGainEnv = fGainEnv + (target - fGainEnv) * fAttack;
            else
                fGainEnv = fGainEnv + (target - fGainEnv) * fRelease;
        }
        else
        {
            // the other styles smooth the gain reduction in dB
            if (style == kBus)
            {
                fGrF = fGrF + (t - fGrF) * (t > fGrF ? fAttack : fRelease);
                fGrS = fGrS + (fGrF - fGrS) * (fGrF > fGrS ? fBusSatt : fBusSrel);
                fGr = std::max(fGrF, fGrS);
            }
            else if (style == kFet)
                fGr = fGr + (t - fGr) * (t > fGr ? fFetAtt : fRelease);
            else if (style == kUpward)
                fGr = fGr + (t - fGr) * (t > fGr ? fAttack : fRelease);
            else
            {
                if (t > fGrO1)
                    fGrO1 = fGrO1 + (t - fGrO1) * std::min(1.0, fOptoAtt * (1.0 + (t - fGrO1) / 6.0));
                else
                    fGrO1 = fGrO1 + (t - fGrO1) * fRelease;
                if (t > fGrO2)
                    fGrO2 = fGrO2 + (t - fGrO2) * std::min(1.0, fOptoAtt * (1.0 + (t - fGrO2) / 6.0));
                else
                    fGrO2 = fGrO2 + (t - fGrO2) * fOptoSrel / (1.0 + fGrMem / 6.0);
                fGr = 0.5 * (fGrO1 + fGrO2);
                fGrMem = fGrMem + (fGr - fGrMem) * fMemTc;
            }
            fGainEnv = std::exp(-fGr * kDbToLin);
        }

        const double makeup = std::exp(fP.makeupDb * kDbToLin);
        const double dryL = l, dryR = r;
        if (style == kFet)
        {
            // FET colour: a cubic curve (6 dB of headroom, linear above) on the compressed
            // signal, odd harmonics only, from 3 dB of reduction, full at 18 dB
            const double amt = 0.5 * std::min(1.0, std::max(0.0, fGr - 3.0) / 15.0);
            l = fet(l * fGainEnv * 2.0, amt) * 0.5 * makeup;
            r = fet(r * fGainEnv * 2.0, amt) * 0.5 * makeup;
        }
        else
        {
            l = l * fGainEnv * makeup;
            r = r * fGainEnv * makeup;
        }
        if (fDelta)
        {
            l = dryL - l / makeup;
            r = dryR - r / makeup;
        }
        else if (fMixOn)
        {
            // parallel compression: the dry signal under the compressed one
            l = dryL + (l - dryL) * fMix;
            r = dryR + (r - dryR) * fMix;
        }
        if (fMs != 0)
        {
            // back to left / right (Delta: the processed half alone)
            if (fDelta)
                other = 0.0;
            const double x = l;
            if (fMs == 1)
            {
                l = x + other;
                r = x - other;
            }
            else
            {
                l = other + x;
                r = other - x;
            }
        }
    }

    double gain() const noexcept { return fGainEnv; }
    double inputAbs() const noexcept { return fInputAbs; }
    const Params& params() const noexcept { return fP; }

private:
    static double fet(double x, double amt) noexcept
    {
        if (std::fabs(x) < 1.0)
            return x - amt * x * x * x / 3.0;
        const double s = x > 0.0 ? 1.0 : -1.0; // EEL2 sign(); |x| >= 1 here
        return s * (1.0 - amt / 3.0) + (x - s) * (1.0 - amt);
    }

    Params fP;
    double fSr = 48000.0, fAttack = 1.0, fRelease = 1.0, fGainEnv = 1.0, fInputAbs = 0.0;
    bool fHpfOn = false, fMixOn = false, fExternal = false, fDelta = false;
    int fMs = 0;
    double fMix = 1.0, fFetAtt = 1.0, fOptoAtt = 0.5, fRmsTc = 1.0, fBusSatt = 1.0, fBusSrel = 1.0, fOptoSrel = 0.1,
           fMemTc = 1.0;
    double fB0 = 1.0, fB1 = 0.0, fA1 = 0.0, fA2 = 0.0;
    double fGr = 0.0, fGrF = 0.0, fGrS = 0.0, fGrO1 = 0.0, fGrO2 = 0.0, fGrMem = 0.0, fDetMs = 0.0;
    double fHpL1 = 0.0, fHpL2 = 0.0, fHpR1 = 0.0, fHpR2 = 0.0;
};

} // namespace nulldynamics
