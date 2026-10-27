/*
 * NULL JSFX - Bass Management, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/bass_management.jsfx, computed in double in the EEL2 order: the
 * mains of a surround bed (12 pins, all but the LFE) high-passed at the Crossover by a 4th-order
 * Linkwitz-Riley, their sum low-passed by the matching LR4 into the LFE pin (the sub) with the
 * LFE channel (+10 dB or 0 dB) and Sub Level. Mains Full Range keeps them whole; Listen plays the
 * sub or the mains alone.
 */

#pragma once

#include <cmath>
#include <cstring>

namespace nullbm {

class BassManagementProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::memset(fHs, 0, sizeof(fHs));
        fZ1 = fZ2 = fW1 = fW2 = 0;
    }

    // @slider
    void setParams(double bed, double crossover, double mains, double lfeGain, double subDb, double listen, double outDb)
    {
        fBed = int(bed);
        fFull = mains >= 0.5;
        fLfeG = lfeGain >= 0.5 ? std::pow(10.0, 10.0 / 20) : 1;
        fSubG = std::pow(10.0, subDb / 20);
        fListen = int(listen);
        fOut = std::pow(10.0, outDb / 20);
        const double k = std::tan(M_PI * std::fmin(crossover, 0.45 * fSr) / fSr);
        const double n = 1 / (1 + std::sqrt(2.0) * k + k * k);
        fLb0 = k * k * n; fLb1 = 2 * fLb0; fLb2 = fLb0;
        fA1 = 2 * (k * k - 1) * n;
        fA2 = (1 - std::sqrt(2.0) * k + k * k) * n;
        fHb0 = n; fHb1 = -2 * fHb0; fHb2 = fHb0;
    }

    // @sample: s[12] in, o[12] out
    void processSample(const double* s, double* o)
    {
        double i[12];
        for (int k = 0; k < 6; ++k)
            i[k] = s[k];
        i[6] = fBed >= 1 ? s[6] : 0; i[7] = fBed >= 1 ? s[7] : 0;
        for (int k = 8; k < 12; ++k)
            i[k] = fBed == 2 ? s[k] : 0;
        const double sub = (lp(i[0] + i[1] + i[2] + i[4] + i[5] + i[6] + i[7] + i[8] + i[9] + i[10] + i[11]) + i[3] * fLfeG) * fSubG;
        if (!fFull)
            for (int k = 0; k < 12; ++k)
                if (k != 3) i[k] = hp(i[k], fHs[k]);
        const double mg = fListen == 1 ? 0 : fOut;
        for (int k = 0; k < 12; ++k)
            o[k] = i[k] * mg;
        o[3] = fListen == 2 ? 0 : sub * fOut;
    }

private:
    double hp(double x, double* s)
    {
        double y = fHb0 * x + s[0];
        s[0] = fHb1 * x - fA1 * y + s[1];
        s[1] = fHb2 * x - fA2 * y;
        x = y;
        y = fHb0 * x + s[2];
        s[2] = fHb1 * x - fA1 * y + s[3];
        s[3] = fHb2 * x - fA2 * y;
        return y;
    }

    double lp(double x)
    {
        double y = fLb0 * x + fZ1;
        fZ1 = fLb1 * x - fA1 * y + fZ2;
        fZ2 = fLb2 * x - fA2 * y;
        x = y;
        y = fLb0 * x + fW1;
        fW1 = fLb1 * x - fA1 * y + fW2;
        fW2 = fLb2 * x - fA2 * y;
        return y;
    }

    double fSr = 44100;
    int fBed = 2, fListen = 0;
    bool fFull = false;
    double fLfeG = 1, fSubG = 1, fOut = 1;
    double fLb0 = 0, fLb1 = 0, fLb2 = 0, fHb0 = 0, fHb1 = 0, fHb2 = 0, fA1 = 0, fA2 = 0;
    double fHs[12][4] {};
    double fZ1 = 0, fZ2 = 0, fW1 = 0, fW2 = 0;
};

} // namespace nullbm
