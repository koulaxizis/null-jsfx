/*
 * NULL JSFX - Transient Shaper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/transient_shaper.jsfx, computed in double like EEL2: a fast
 * (1 / 20 ms) and a slow (20 / 150 ms) envelope of the stereo-linked peak; where the fast one
 * runs ahead (relative to the slow one, 0..1) the Attack gain applies, elsewhere the Sustain
 * gain, blended with the dry signal by Mix. Speed halves (Fast) or doubles (Slow) the four
 * times; Focus Low / High (12 dB/oct RBJ high- and low-pass, off at 20 Hz / 20 kHz) limit the
 * detection and the shaping to a band (the rest passes untouched); then Output gain (skipped at
 * 0 dB) and an optional soft clip (linear to 0.7, bending towards 1.0).
 */

#pragma once

#include <cmath>

namespace nulltransient {

class TransientProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fFast = fSlow = 0.0;
        fTrans = 0.0;
        fTotal = 1.0;
        fHl1 = fHl2 = fHr1 = fHr2 = 0.0;
        fLl1 = fLl2 = fLr1 = fLr2 = 0.0;
    }

    // @slider
    void setParams(double attackDb, double sustainDb, double mixPct, int speed = 1, double focusLow = 20.0,
                   double focusHigh = 20000.0, bool clip = false, double outDb = 0.0)
    {
        fMix = mixPct / 100.0;
        fAttackBoost = std::exp(attackDb * 0.11512925464970228420);
        fSustainBoost = std::exp(sustainDb * 0.11512925464970228420);
        double fa = 0.001, fr = 0.020, sa = 0.020, sr = 0.150;
        if (speed == 0)
        {
            fa = 0.0005; fr = 0.010; sa = 0.010; sr = 0.075;
        }
        if (speed == 2)
        {
            fa = 0.002; fr = 0.040; sa = 0.040; sr = 0.300;
        }
        fFa = 1.0 - std::exp(-1.0 / (fa * fSr));
        fFr = 1.0 - std::exp(-1.0 / (fr * fSr));
        fSa = 1.0 - std::exp(-1.0 / (sa * fSr));
        fSrc = 1.0 - std::exp(-1.0 / (sr * fSr));

        fLo = focusLow > 20.0;
        fHi = focusHigh < 20000.0;
        fFocus = fLo || fHi;
        if (fLo)
        {
            const double w = 2.0 * 3.141592653589793 * std::fmin(focusLow, fSr * 0.45) / fSr;
            const double cs = std::cos(w), al = std::sin(w) / (2.0 * std::sqrt(0.5)), a0 = 1.0 + al;
            fHb0 = (1.0 + cs) / 2.0 / a0;
            fHb1 = -(1.0 + cs) / a0;
            fHa1 = -2.0 * cs / a0;
            fHa2 = (1.0 - al) / a0;
        }
        if (fHi)
        {
            const double w = 2.0 * 3.141592653589793 * std::fmin(focusHigh, fSr * 0.45) / fSr;
            const double cs = std::cos(w), al = std::sin(w) / (2.0 * std::sqrt(0.5)), a0 = 1.0 + al;
            fLb0 = (1.0 - cs) / 2.0 / a0;
            fLb1 = (1.0 - cs) / a0;
            fLa1 = -2.0 * cs / a0;
            fLa2 = (1.0 - al) / a0;
        }
        fOutOn = std::fabs(outDb) >= 0.00001; // EEL2's fuzzy !=
        fOutGain = std::exp(outDb * 0.11512925464970228420);
        fClip = clip;
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        double bl = l, br = r;
        if (fFocus)
        {
            if (fLo)
            {
                double y = fHb0 * bl + fHl1;
                fHl1 = fHb1 * bl - fHa1 * y + fHl2;
                fHl2 = fHb0 * bl - fHa2 * y;
                bl = y;
                y = fHb0 * br + fHr1;
                fHr1 = fHb1 * br - fHa1 * y + fHr2;
                fHr2 = fHb0 * br - fHa2 * y;
                br = y;
            }
            if (fHi)
            {
                double y = fLb0 * bl + fLl1;
                fLl1 = fLb1 * bl - fLa1 * y + fLl2;
                fLl2 = fLb0 * bl - fLa2 * y;
                bl = y;
                y = fLb0 * br + fLr1;
                fLr1 = fLb1 * br - fLa1 * y + fLr2;
                fLr2 = fLb0 * br - fLa2 * y;
                br = y;
            }
        }
        const double al = bl < 0.0 ? -bl : bl, ar = br < 0.0 ? -br : br;
        const double in = al > ar ? al : ar;
        fIn = in;
        if (in > fFast)
            fFast = fFast + fFa * (in - fFast);
        else
            fFast = fFast + fFr * (in - fFast);
        if (in > fSlow)
            fSlow = fSlow + fSa * (in - fSlow);
        else
            fSlow = fSlow + fSrc * (in - fSlow);
        double t = fSlow > 0.0001 ? (fFast - fSlow) / fSlow : 0.0;
        t = t > 1.0 ? 1.0 : t;
        t = t < 0.0 ? 0.0 : t;
        fTrans = t;
        const double s = 1.0 - t;
        fTotal = 1.0 + (fAttackBoost - 1.0) * t + (fSustainBoost - 1.0) * s;
        double ol, orr;
        if (fFocus)
        {
            ol = l + bl * (fTotal - 1.0);
            orr = r + br * (fTotal - 1.0);
        }
        else
        {
            ol = l * fTotal;
            orr = r * fTotal;
        }
        l = l * (1.0 - fMix) + ol * fMix;
        r = r * (1.0 - fMix) + orr * fMix;
        if (fOutOn)
        {
            l *= fOutGain;
            r *= fOutGain;
        }
        if (fClip)
        {
            l = softClip(l);
            r = softClip(r);
        }
    }

    // linear up to 0.7, then bending smoothly towards 1.0
    static double softClip(double x) noexcept
    {
        const double a = std::fabs(x);
        if (!(a > 0.7))
            return x;
        const double u = (a - 0.7) * (1.0 / 0.3);
        return (x > 0.0 ? 1.0 : -1.0) * (0.7 + 0.3 * u / (1.0 + u));
    }

    double input() const noexcept { return fIn; }
    double transient() const noexcept { return fTrans; }
    // the gain after Mix and Output (for the display)
    double gain() const noexcept { return (1.0 - fMix + fTotal * fMix) * (fOutOn ? fOutGain : 1.0); }

private:
    double fSr = 48000.0, fFast = 0.0, fSlow = 0.0, fTrans = 0.0, fTotal = 1.0, fIn = 0.0;
    bool fFocus = false, fLo = false, fHi = false, fOutOn = false, fClip = false;
    double fOutGain = 1.0;
    double fHb0 = 0, fHb1 = 0, fHa1 = 0, fHa2 = 0, fLb0 = 0, fLb1 = 0, fLa1 = 0, fLa2 = 0;
    double fHl1 = 0, fHl2 = 0, fHr1 = 0, fHr2 = 0, fLl1 = 0, fLl2 = 0, fLr1 = 0, fLr2 = 0;
    double fMix = 1.0, fAttackBoost = 1.0, fSustainBoost = 1.0, fFa = 1.0, fFr = 1.0, fSa = 1.0, fSrc = 1.0;
};

} // namespace nulltransient
