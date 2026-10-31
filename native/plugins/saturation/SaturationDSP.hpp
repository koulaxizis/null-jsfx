/*
 * NULL JSFX - Saturation, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/saturation.jsfx, computed in double like EEL2: the input is
 * driven (1 + 3 x Drive) into one of seven shapers (Tape, Tube, Overdrive, Fuzz, Bitcrush,
 * Waveshaper, Fold), the wet signal gets a tilt EQ around 800 Hz (Tone, +-12 dB), then a dry/wet mix.
 * Input and Output gains (skipped at 0 dB), Emphasis (a first-order tilt before the shaper and
 * its exact inverse after it, skipped at 0 dB) and Oversampling (2x / 4x through the halfband
 * stages of plugins/clipper, latency 31 / 37 samples, the dry signal delayed to match).
 * Channels (appended in 2.7): Mid / Side saturates only that half of the mid/side pair, on both
 * channels; the other half is delayed by the oversampling latency and added back after.
 * Focus and Density (appended in 2.7, off by default): Focus is a +9 dB peak (Q 0.7) at 100 Hz,
 * 1 kHz or 3 kHz before the shaper and its exact inverse after it; Density follows the shaper
 * (at its rate), y (1 + d) / (1 + d |y|).
 */

#pragma once

#include <cmath>
#include <cstdint>

#include "../clipper/ClipperDSP.hpp"

namespace nullsaturation {

// the shaper for mode 0..6 with drive 0..1 (in = already driven input), as in @sample
inline double shape(double in, int mode, double drive) noexcept
{
    double wet;
    if (mode == 0)
    {
        in = std::fmax(-1.0 / 0.6, std::fmin(1.0 / 0.6, in));
        wet = in * (1.0 - std::fabs(in) * 0.3);
    }
    else if (mode == 1)
    {
        wet = in - (in * in * in) / 3.0;
        if (wet > 1.0)
            wet = 1.0;
        else if (wet < -1.0)
            wet = -1.0;
    }
    else if (mode == 2)
    {
        const double a = in < 0.0 ? -in : in;
        wet = a > 1.0 ? (in < 0.0 ? -1.0 : 1.0) : in;
        if (wet > 0.5)
            wet = 1.0 - (1.0 - wet) * (1.0 - wet);
        if (wet < -0.5)
            wet = -1.0 + (1.0 + wet) * (1.0 + wet);
    }
    else if (mode == 3)
    {
        const double a = in < 0.0 ? -in : in;
        wet = a > 0.1 ? (in > 0.0 ? 1.0 : -1.0) : 0.0;
    }
    else if (mode == 4)
    {
        double bits = 8.0 - std::floor(drive * 6.0);
        if (bits < 2.0)
            bits = 2.0;
        const double quants = std::pow(2.0, bits) - 1.0;
        wet = std::floor(in * quants + 0.5) / quants;
    }
    else if (mode == 5)
    {
        wet = in;
        if (wet > 1.0)
            wet = 2.0 - wet;
        if (wet < -1.0)
            wet = -2.0 - wet;
        if (wet > 1.0)
            wet = 2.0 - wet;
        if (wet < -1.0)
            wet = -2.0 - wet;
    }
    else // Fold: sine wavefolder, Drive adds folds (1x to 2x on top of the drive)
        wet = std::sin(in * (1.0 + drive) * 3.141592653589793 / 2.0);
    return wet;
}

struct Params {
    double drive = 30.0, tone = 0.0, mix = 50.0;
    int mode = 0;
    double input = 0.0, output = 0.0;
    int oversampling = 0; // 0 off, 1 = 2x, 2 = 4x
    double emphasis = 0.0;
    int channels = 0; // 0 stereo, 1 mid, 2 side
    int focus = 0;    // 0 off, 1 bass, 2 mid, 3 presence
    double density = 0.0;
};

// latency in samples for an Oversampling setting (the JSFX's pdc_delay)
inline int latencyFor(int os) noexcept { return os <= 0 ? 0 : os == 1 ? 31 : 37; }

class SaturationProcessor {
public:
    SaturationProcessor()
    {
        nullclipper::halfbandDesign(fE1, fD1, 63, 7.5);
        nullclipper::halfbandDesign(fE2, fD2, 23, 8.0);
    }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fToneL = fToneR = 0.0;
        fAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979 * 800.0 / fSr);
        fEmXl = fEmYl = fEmXr = fEmYr = 0.0;
        fDeXl = fDeYl = fDeXr = fDeYr = 0.0;
        clearFocus();
        fOsPrev = -1;
        fPw = 0;
        fDw = 0;
        for (double& v : fMsBuf)
            v = 0.0;
    }

    // @slider (the 4-argument form keeps the newer sliders at their defaults)
    void setParams(double drivePct, double toneDb, double mixPct, int mode)
    {
        Params p;
        p.drive = drivePct;
        p.tone = toneDb;
        p.mix = mixPct;
        p.mode = mode;
        setParams(p);
    }

    void setParams(const Params& p)
    {
        fDrive = p.drive / 100.0;
        fMix = p.mix / 100.0;
        fMode = p.mode;
        fAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979 * 800.0 / fSr);
        fHi = std::pow(10.0, p.tone / 40.0);
        fLo = 1.0 / fHi;
        const double in = p.input, out = p.output, em = p.emphasis;
        fInOn = std::fabs(in) >= 0.00001;
        fGin = std::pow(10.0, in / 20.0);
        fOutOn = std::fabs(out) >= 0.00001;
        fGout = std::pow(10.0, out / 20.0);
        // Emphasis: H(s) = (hi s + lo w) / (s + w) at w = 2 pi 800 Hz (bilinear, prewarped)
        fEmOn = std::fabs(em) >= 0.00001;
        const double hi = std::pow(10.0, em / 40.0), lo = 1.0 / hi;
        const double t = std::tan(nullclipper::kPi * std::fmin(800.0, fSr * 0.45) / fSr);
        fB0 = hi + lo * t;
        fB1 = lo * t - hi;
        fA0 = 1.0 + t;
        fA1 = t - 1.0;
        if (!fEmOn)
        {
            fEmXl = fEmYl = fEmXr = fEmYr = 0.0;
            fDeXl = fDeYl = fDeXr = fDeYr = 0.0;
        }
        fMs = p.channels;
        fDnOn = p.density > 0.0;
        fDn = p.density / 100.0;
        fFoOn = p.focus > 0;
        if (fFoOn)
        {
            const double w = 2.0 * nullclipper::kPi * std::fmin(p.focus == 1 ? 100.0 : p.focus == 2 ? 1000.0 : 3000.0, 0.45 * fSr) / fSr;
            const double a = std::pow(10.0, 9.0 / 40.0);
            const double al = std::sin(w) / (2.0 * 0.7);
            fP0 = 1.0 + al * a;
            fP1 = -2.0 * std::cos(w);
            fP2 = 1.0 - al * a;
            fQ0 = 1.0 + al / a;
            fQ2 = 1.0 - al / a;
        }
        else
            clearFocus();
        const int os = p.oversampling;
        fOsf = os <= 0 ? 1 : os == 1 ? 2 : 4;
        fLat = latencyFor(os);
        fPad = os >= 2 ? 2 : 0;
        // a new rate starts every filter and delay from silence
        if (fOsf != fOsPrev)
        {
            fL.reset();
            fR.reset();
            fPw = 0;
            fDw = 0;
            for (double& v : fMsBuf)
                v = 0.0;
            fOsPrev = fOsf;
        }
    }

    int latency() const noexcept { return fLat; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double dl = inL[i], dr = inR[i], other = 0.0;
            if (fMs != 0)
            {
                const double m = (dl + dr) * 0.5;
                const double sd = (dl - dr) * 0.5;
                if (fMs == 1)
                {
                    dl = dr = m;
                    other = sd;
                }
                else
                {
                    dl = dr = sd;
                    other = m;
                }
            }
            double xl = dl, xr = dr;
            if (fInOn)
            {
                xl *= fGin;
                xr *= fGin;
            }
            if (fEmOn)
            {
                double v = (fB0 * xl + fB1 * fEmXl - fA1 * fEmYl) / fA0;
                fEmXl = xl; fEmYl = v; xl = v;
                v = (fB0 * xr + fB1 * fEmXr - fA1 * fEmYr) / fA0;
                fEmXr = xr; fEmYr = v; xr = v;
            }
            if (fFoOn)
            {
                double v = (fP0 * xl + fP1 * fFa[0] + fP2 * fFa[1] - fP1 * fFa[2] - fQ2 * fFa[3]) / fQ0;
                fFa[1] = fFa[0]; fFa[0] = xl; fFa[3] = fFa[2]; fFa[2] = v; xl = v;
                v = (fP0 * xr + fP1 * fFa[4] + fP2 * fFa[5] - fP1 * fFa[6] - fQ2 * fFa[7]) / fQ0;
                fFa[5] = fFa[4]; fFa[4] = xr; fFa[7] = fFa[6]; fFa[6] = v; xr = v;
            }
            xl = xl * (1.0 + fDrive * 3.0);
            xr = xr * (1.0 + fDrive * 3.0);

            double wl, wr;
            if (fOsf > 1)
            {
                double o0, o1;
                fL.u1.process(xl, fE1, 15, o0, o1); fL.a1[0] = o0; fL.a1[1] = o1;
                fR.u1.process(xr, fE1, 15, o0, o1); fR.a1[0] = o0; fR.a1[1] = o1;
                if (fOsf > 2)
                    for (int k = 0; k < 2; ++k)
                    {
                        fL.u2.process(fL.a1[k], fE2, 5, o0, o1); fL.a2[2 * k] = o0; fL.a2[2 * k + 1] = o1;
                        fR.u2.process(fR.a1[k], fE2, 5, o0, o1); fR.a2[2 * k] = o0; fR.a2[2 * k + 1] = o1;
                    }
                double* tl = fOsf > 2 ? fL.a2 : fL.a1;
                double* tr = fOsf > 2 ? fR.a2 : fR.a1;
                for (int k = 0; k < fOsf; ++k)
                {
                    double cxl = tl[k], cxr = tr[k];
                    if (fPad > 0)
                    {
                        fL.pd[fPw] = cxl;
                        fR.pd[fPw] = cxr;
                        const int j = (fPw - fPad + 8) % 8;
                        cxl = fL.pd[j];
                        cxr = fR.pd[j];
                        fPw = (fPw + 1) % 8;
                    }
                    tl[k] = dens(shape(cxl, fMode, fDrive));
                    tr[k] = dens(shape(cxr, fMode, fDrive));
                }
                if (fOsf > 2)
                    for (int k = 0; k < 2; ++k)
                    {
                        fL.a1[k] = fL.d2.process(fL.a2[2 * k], fL.a2[2 * k + 1], fD2);
                        fR.a1[k] = fR.d2.process(fR.a2[2 * k], fR.a2[2 * k + 1], fD2);
                    }
                wl = fL.d1.process(fL.a1[0], fL.a1[1], fD1);
                wr = fR.d1.process(fR.a1[0], fR.a1[1], fD1);
                // dry signal delayed by the latency
                fL.dr[fDw] = dl;
                fR.dr[fDw] = dr;
                const int j = (fDw - fLat + 64) % 64;
                dl = fL.dr[j];
                dr = fR.dr[j];
                if (fMs != 0)
                {
                    fMsBuf[fDw] = other;
                    other = fMsBuf[j];
                }
                fDw = (fDw + 1) % 64;
            }
            else
            {
                wl = dens(shape(xl, fMode, fDrive));
                wr = dens(shape(xr, fMode, fDrive));
            }

            if (fFoOn)
            {
                double v = (fQ0 * wl + fP1 * fFb[0] + fQ2 * fFb[1] - fP1 * fFb[2] - fP2 * fFb[3]) / fP0;
                fFb[1] = fFb[0]; fFb[0] = wl; fFb[3] = fFb[2]; fFb[2] = v; wl = v;
                v = (fQ0 * wr + fP1 * fFb[4] + fQ2 * fFb[5] - fP1 * fFb[6] - fP2 * fFb[7]) / fP0;
                fFb[5] = fFb[4]; fFb[4] = wr; fFb[7] = fFb[6]; fFb[6] = v; wr = v;
            }

            if (fEmOn)
            {
                double v = (fA0 * wl + fA1 * fDeYl - fB1 * fDeXl) / fB0;
                fDeYl = wl; fDeXl = v; wl = v;
                v = (fA0 * wr + fA1 * fDeYr - fB1 * fDeXr) / fB0;
                fDeYr = wr; fDeXr = v; wr = v;
            }

            fToneL += fAlpha * (wl - fToneL);
            fToneR += fAlpha * (wr - fToneR);
            wl = fToneL * fLo + (wl - fToneL) * fHi;
            wr = fToneR * fLo + (wr - fToneR) * fHi;
            double yl = dl * (1.0 - fMix) + wl * fMix;
            double yr = dr * (1.0 - fMix) + wr * fMix;
            if (fOutOn)
            {
                yl *= fGout;
                yr *= fGout;
            }
            if (fMs != 0)
            {
                // back to left / right
                const double x = yl;
                if (fMs == 1)
                {
                    yl = x + other;
                    yr = x - other;
                }
                else
                {
                    yl = other + x;
                    yr = other - x;
                }
            }
            outL[i] = static_cast<T>(yl);
            outR[i] = static_cast<T>(yr);
        }
    }

private:
    double dens(double y) const noexcept { return fDnOn ? y * (1.0 + fDn) / (1.0 + fDn * std::fabs(y)) : y; }
    void clearFocus() noexcept
    {
        for (int k = 0; k < 8; ++k)
            fFa[k] = fFb[k] = 0.0;
    }

    bool fFoOn = false, fDnOn = false;
    double fDn = 0.0, fP0 = 1.0, fP1 = 0.0, fP2 = 0.0, fQ0 = 1.0, fQ2 = 0.0;
    double fFa[8] = {}, fFb[8] = {}; // per side: x1, x2, y1, y2 (left 0-3, right 4-7)
    double fSr = 48000.0, fAlpha = 0.0, fToneL = 0.0, fToneR = 0.0;
    double fDrive = 0.3, fMix = 0.5, fHi = 1.0, fLo = 1.0;
    int fMode = 0;
    bool fInOn = false, fOutOn = false, fEmOn = false;
    double fGin = 1.0, fGout = 1.0;
    double fB0 = 1.0, fB1 = 0.0, fA0 = 1.0, fA1 = 0.0;
    double fEmXl = 0, fEmYl = 0, fEmXr = 0, fEmYr = 0, fDeXl = 0, fDeYl = 0, fDeXr = 0, fDeYr = 0;
    double fE1[32] = {}, fD1[32] = {}, fE2[12] = {}, fD2[12] = {};
    nullclipper::Channel fL, fR;
    int fOsf = 1, fOsPrev = -1, fLat = 0, fPad = 0, fPw = 0, fDw = 0, fMs = 0;
    double fMsBuf[64] = {};
};

} // namespace nullsaturation
