/*
 * NULL JSFX - Reference, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/reference.jsfx, computed in double in the EEL2 order: the
 * track (inputs 1/2) and the reference (3/4) are K-weighted and measured as short-term
 * loudness every 100 ms; the reference's gain follows the difference (Level Match) plus Trim.
 * Listen switches between them with a 10 ms crossfade; Monitor folds to mono or plays the
 * side. The stereo correlation of each is kept for the display.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nullref {

class ReferenceProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        const double pi = M_PI;
        double k = std::tan(pi * 1681.974450955533 / sr);
        const double vh = std::pow(10.0, 3.999843853973347 / 20);
        const double vb = std::pow(vh, 0.4996667741545416);
        double q = 0.7071752369554196;
        const double a0 = 1 + k / q + k * k;
        fB0 = (vh + vb * k / q + k * k) / a0;
        fB1 = 2 * (k * k - vh) / a0;
        fB2 = (vh - vb * k / q + k * k) / a0;
        fA1 = 2 * (k * k - 1) / a0;
        fA2 = (1 - k / q + k * k) / a0;
        k = std::tan(pi * 38.13547087602444 / sr);
        q = 0.5003270373238773;
        const double c0 = 1 + k / q + k * k;
        fC1 = 2 * (k * k - 1) / c0;
        fC2 = (1 - k / q + k * k) / c0;
        std::memset(fZ, 0, sizeof(fZ));
        std::memset(fET, 0, sizeof(fET));
        std::memset(fER, 0, sizeof(fER));
        fN = std::floor(sr * 0.1 + 0.5);
        fCnt = 0;
        fW = 0;
        fAccT = fAccR = 0;
        fLt = fLr = -200;
        fG = 0;
        fXf = 0;
        fXs = 1 / (0.01 * sr);
        fCa = std::exp(-1 / (3 * sr));
        fTlr = fTll = fTrr = fRlr = fRll = fRrr = 0;
    }

    // @slider
    void setParams(double listen, double match, double trim, double monitor)
    {
        fListen = listen;
        fMatch = match;
        fTrim = trim;
        fMon = int(monitor);
        gain();
    }

    void processSample(double& spl0, double& spl1, double spl2, double spl3)
    {
        const double tl = kw(spl0, fZ[0]), tr = kw(spl1, fZ[1]), rl = kw(spl2, fZ[2]), rr = kw(spl3, fZ[3]);
        fAccT += tl * tl + tr * tr;
        fAccR += rl * rl + rr * rr;
        fCnt += 1;
        if (fCnt >= fN)
        {
            fCnt = 0;
            subblock();
        }
        fTlr = fCa * fTlr + (1 - fCa) * spl0 * spl1;
        fTll = fCa * fTll + (1 - fCa) * spl0 * spl0;
        fTrr = fCa * fTrr + (1 - fCa) * spl1 * spl1;
        fRlr = fCa * fRlr + (1 - fCa) * spl2 * spl3;
        fRll = fCa * fRll + (1 - fCa) * spl2 * spl2;
        fRrr = fCa * fRrr + (1 - fCa) * spl3 * spl3;

        fXf = fListen > fXf ? std::min(fListen, fXf + fXs) : std::max(fListen, fXf - fXs);
        const double ol = spl0 * (1 - fXf) + spl2 * fGr * fXf;
        const double orr = spl1 * (1 - fXf) + spl3 * fGr * fXf;
        if (fMon == 1)
            spl0 = spl1 = (ol + orr) * 0.5;
        else if (fMon == 2)
        {
            spl0 = (ol - orr) * 0.5;
            spl1 = spl0;
        }
        else
        {
            spl0 = ol;
            spl1 = orr;
        }
    }

    // for the display
    double trackLufs() const { return fLt; }
    double referenceLufs() const { return fLr; }
    double referenceGain() const { return fGr; }
    static double corr(double lr, double ll, double rr) { return ll * rr > 0 ? lr / std::sqrt(ll * rr) : 0; }
    double trackCorr() const { return corr(fTlr, fTll, fTrr); }
    double referenceCorr() const { return corr(fRlr, fRll, fRrr); }

private:
    double kw(double x, double* z)
    {
        const double y = fB0 * x + z[0];
        z[0] = fB1 * x - fA1 * y + z[1];
        z[1] = fB2 * x - fA2 * y;
        const double k = y + z[2];
        z[2] = -2 * y - fC1 * k + z[3];
        z[3] = y - fC2 * k;
        return k;
    }

    static double db(double p) { return p > 0 ? -0.691 + 10 * std::log10(p) : -200; }

    void gain() { fGr = std::pow(10.0, ((fMatch != 0 ? fG : 0) + fTrim) / 20); }

    void subblock()
    {
        fET[fW] = fAccT;
        fER[fW] = fAccR;
        fAccT = fAccR = 0;
        fW = (fW + 1) % 30;
        double st = 0, sr = 0;
        for (int i = 0; i < 30; ++i)
        {
            st += fET[i];
            sr += fER[i];
        }
        fLt = db(st / (30 * fN));
        fLr = db(sr / (30 * fN));
        if (fLt > -60 && fLr > -60)
            fG += (std::max(-24.0, std::min(24.0, fLt - fLr)) - fG) * 0.1;
        gain();
    }

    double fB0 = 1, fB1 = 0, fB2 = 0, fA1 = 0, fA2 = 0, fC1 = 0, fC2 = 0;
    double fZ[4][4] = {}, fET[30] = {}, fER[30] = {};
    double fN = 4800, fCnt = 0, fAccT = 0, fAccR = 0, fLt = -200, fLr = -200, fG = 0, fGr = 1;
    int fW = 0, fMon = 0;
    double fXf = 0, fXs = 0, fCa = 0, fListen = 0, fMatch = 1, fTrim = 0;
    double fTlr = 0, fTll = 0, fTrr = 0, fRlr = 0, fRll = 0, fRrr = 0;
};

} // namespace nullref
