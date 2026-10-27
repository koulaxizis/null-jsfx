/*
 * NULL JSFX - Surround Panner, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/surround_panner.jsfx, computed in double in the EEL2 order: a
 * mono or stereo input panned into a 5.1, 7.1 or 7.1.4 bed (12 pins: L R C LFE Ls Rs Lrs Rrs
 * Ltf Rtf Ltr Rtr) by pairwise power-normalised VBAP on the ear-level ring, with a Center
 * crossfade into a phantom centre, a seven-point spread for a mono input, Elevation moving the
 * power to the four tops (7.1.4), and the mono sum through a 120 Hz Linkwitz-Riley low-pass to
 * the LFE. The gains glide over about 20 ms.
 */

#pragma once

#include <cmath>
#include <cstring>

namespace nullpan {

static constexpr int kPins = 12;

class SurroundPannerProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::memset(fGL, 0, sizeof(fGL));
        std::memset(fGR, 0, sizeof(fGR));
        std::memset(fSL, 0, sizeof(fSL));
        std::memset(fSR, 0, sizeof(fSR));
        std::memset(fSpk, 0, sizeof(fSpk));
        std::memset(fTmp, 0, sizeof(fTmp));
        fInit = false;
        fZ1 = fZ2 = fW1 = fW2 = 0;
    }

    // @slider
    void setParams(double format, double input, double azimuth, double elevation, double spread, double center,
                   double lfeDb, double outDb)
    {
        fFmt = int(format);
        fStereo = input >= 0.5;
        fElev = elevation;
        fCen = center / 100;
        fOutg = std::pow(10.0, outDb / 20);
        fLfe = lfeDb > -60 ? std::pow(10.0, lfeDb / 20) * fOutg : 0;
        if (fStereo)
        {
            source(azimuth - spread / 2, 0, fGL);
            source(azimuth + spread / 2, 0, fGR);
        }
        else
        {
            source(azimuth, spread, fGL);
            std::memset(fGR, 0, sizeof(fGR));
        }
        if (fFmt == 0)
            for (int k = 6; k < 12; ++k) fGL[k] = fGR[k] = 0;
        if (fFmt == 1)
            for (int k = 8; k < 12; ++k) fGL[k] = fGR[k] = 0;
        const double k = std::tan(M_PI * std::fmin(120.0, 0.45 * fSr) / fSr);
        const double n = 1 / (1 + std::sqrt(2.0) * k + k * k);
        fB0 = k * k * n; fB1 = 2 * fB0; fB2 = fB0;
        fA1 = 2 * (k * k - 1) * n;
        fA2 = (1 - std::sqrt(2.0) * k + k * k) * n;
        if (!fInit)
        {
            std::memcpy(fSL, fGL, sizeof(fSL));
            std::memcpy(fSR, fGR, sizeof(fSR));
            fInit = true;
        }
        fSm = std::exp(-1 / (0.02 * fSr / 3));
    }

    // @sample: in[0..1] in, out[0..11] out
    void processSample(double inL, double inR, double* out)
    {
        for (int k = 0; k < kPins; ++k)
        {
            fSL[k] = fGL[k] + (fSL[k] - fGL[k]) * fSm;
            fSR[k] = fGR[k] + (fSR[k] - fGR[k]) * fSm;
        }
        const double lfeS = fStereo ? (inL + inR) * 0.5 : inL;
        for (int k = 0; k < kPins; ++k)
            out[k] = inL * fSL[k] + inR * fSR[k];
        out[3] = inL * fSL[3] + inR * fSR[3] + lp(lfeS) * fLfe;
    }

    // what the display shows: the power of pin k relative to the Output gain (0..1)
    double pinPower(int k) const
    {
        const double p = std::fmin(1.0, fGL[k] * fGL[k] + fGR[k] * fGR[k]);
        return std::sqrt(p / std::fmax(0.0001, fOutg * fOutg));
    }
    int format() const { return fFmt; }

private:
    // VBAP of the point at azimuth a on the ring of n speakers in fSpk, power x w into dst
    void vbap(double a, int n, double* dst, double w)
    {
        int i = n - 1;
        for (int k = 0; k < n; ++k)
            if (fSpk[k * 2] <= a) i = k;
        if (a < fSpk[0]) i = n - 1;
        int j = i + 1;
        if (j >= n) j = 0;
        double a1 = fSpk[i * 2];
        double a2 = fSpk[j * 2];
        double x = a - a1;
        if (x < 0) x += 360;
        double d = a2 - a1;
        if (d <= 0) d += 360;
        a2 = d * M_PI / 180;
        x *= M_PI / 180;
        d = std::sin(a2);
        const double g1 = (std::cos(x) * std::sin(a2) - std::sin(x) * std::cos(a2)) / d;
        const double g2 = std::sin(x) / d;
        d = g1 * g1 + g2 * g2;
        if (d > 0)
        {
            dst[int(fSpk[i * 2 + 1])] += w * g1 * g1 / d;
            dst[int(fSpk[j * 2 + 1])] += w * g2 * g2 / d;
        }
    }

    // the ring of the ear level (with or without C) or of the tops, into fSpk; returns its size
    int ring(bool top, bool withc)
    {
        static const double tops[] = { -135, 10, -45, 8, 45, 9, 135, 11 };
        static const double r51c[] = { -110, 4, -30, 0, 0, 2, 30, 1, 110, 5 };
        static const double r51[] = { -110, 4, -30, 0, 30, 1, 110, 5 };
        static const double r71c[] = { -150, 6, -90, 4, -30, 0, 0, 2, 30, 1, 90, 5, 150, 7 };
        static const double r71[] = { -150, 6, -90, 4, -30, 0, 30, 1, 90, 5, 150, 7 };
        const double* src;
        int n;
        if (top) { src = tops; n = 4; }
        else if (fFmt == 0) { src = withc ? r51c : r51; n = withc ? 5 : 4; }
        else { src = withc ? r71c : r71; n = withc ? 7 : 6; }
        for (int k = 0; k < n * 2; ++k)
            fSpk[k] = src[k];
        return n;
    }

    void point(double a, double w)
    {
        a = a - std::floor((a + 180) / 360) * 360;
        double e = fFmt == 2 ? std::cos(fElev * M_PI / 180) : 1;
        e *= e;
        if (fCen > 0) vbap(a, ring(false, true), fTmp, w * e * fCen);
        if (fCen < 1) vbap(a, ring(false, false), fTmp, w * e * (1 - fCen));
        if (fFmt == 2 && fElev > 0) vbap(a, ring(true, false), fTmp, w * (1 - e));
    }

    void source(double a, double sp, double* dst)
    {
        std::memset(fTmp, 0, sizeof(fTmp));
        if (sp > 0)
            for (int k = 0; k < 7; ++k)
                point(a + sp * (k / 6.0 - 0.5), 1.0 / 7);
        else
            point(a, 1);
        for (int k = 0; k < kPins; ++k)
            dst[k] = std::sqrt(fTmp[k]) * fOutg;
    }

    double lp(double x)
    {
        double y = fB0 * x + fZ1;
        fZ1 = fB1 * x - fA1 * y + fZ2;
        fZ2 = fB2 * x - fA2 * y;
        x = y;
        y = fB0 * x + fW1;
        fW1 = fB1 * x - fA1 * y + fW2;
        fW2 = fB2 * x - fA2 * y;
        return y;
    }

    double fSr = 44100;
    double fGL[kPins] {}, fGR[kPins] {}, fSL[kPins] {}, fSR[kPins] {};
    double fSpk[16] {}, fTmp[kPins] {};
    bool fInit = false, fStereo = true;
    int fFmt = 0;
    double fElev = 0, fCen = 1, fOutg = 1, fLfe = 0, fSm = 0;
    double fB0 = 0, fB1 = 0, fB2 = 0, fA1 = 0, fA2 = 0;
    double fZ1 = 0, fZ2 = 0, fW1 = 0, fW2 = 0;
};

} // namespace nullpan
