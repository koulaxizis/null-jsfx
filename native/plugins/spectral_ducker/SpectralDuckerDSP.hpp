/*
 * NULL JSFX - Spectral Ducker, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/spectral_ducker.jsfx, computed in double in the EEL2 order:
 * 32 fourth-order key band passes (30 Hz - 18 kHz) follow the key's level per band; every 16
 * samples the excess over Threshold (up to Amount x 24 dB) becomes the cut of a bell on the
 * same band of the track (0.43 of it: the Q 3 bells overlap). Stereo, mid or side; Listen Delta
 * plays what is taken. Zero latency.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullsduck {

static constexpr int kBands = 32;

// the centre of band k (Hz)
inline double bandFreq(int k) { return 30 * std::pow(600.0, k / 31.0); }

class SpectralDuckerProcessor {
public:
    // the JSFX @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::fill(&fKS[0][0], &fKS[0][0] + kBands * 8, 0.0);
        std::fill(fEnv, fEnv + kBands, 0.0);
        std::fill(fGr, fGr + kBands, 0.0);
        std::fill(&fMC[0][0], &fMC[0][0] + kBands * 5, 0.0);
        std::fill(&fMS[0][0], &fMS[0][0] + kBands * 8, 0.0);
        fCnt = 0;
    }

    // the JSFX @slider (update()); v = the ten slider values
    void setParams(const double* v)
    {
        for (int k = 0; k < kBands; ++k)
        {
            const double f = 30 * std::pow(600.0, k / 31.0);
            const double w = 2 * M_PI * std::min(f, fSr * 0.49) / fSr;
            const double al = std::sin(w) / (2 * 4.84);
            fKC[k][0] = al / (1 + al);
            fKC[k][1] = -2 * std::cos(w) / (1 + al);
            fKC[k][2] = (1 - al) / (1 + al);
            fCW[k] = std::cos(w);
            fAL[k] = std::sin(w) / (2 * 3);
            fOn[k] = f >= v[4] && f <= v[5] && f < fSr * 0.45;
        }
        fCut = v[0] * 0.24;
        fThr = v[1];
        fAtt = std::exp(-1 / (v[2] * 0.001 * fSr));
        fRel = std::exp(-1 / (v[3] * 0.001 * fSr));
        fChan = int(v[6]);
        fMix = v[7] / 100;
        fOut = std::pow(10.0, v[8] / 20);
        fDelta = v[9] != 0.0;
        gains();
    }

    void processSample(double& spl0, double& spl1, double key0, double key1)
    {
        const double key = (key0 + key1) * 0.5;
        for (int k = 0; k < kBands; ++k)
        {
            const double* c = fKC[k];
            double* s = fKS[k];
            double y = c[0] * key - c[0] * s[1] - c[1] * s[2] - c[2] * s[3];
            s[1] = s[0]; s[0] = key; s[3] = s[2]; s[2] = y;
            const double x = y;
            y = c[0] * x - c[0] * s[5] - c[1] * s[6] - c[2] * s[7];
            s[5] = s[4]; s[4] = x; s[7] = s[6]; s[6] = y;
            const double a = std::fabs(y);
            fEnv[k] = a > fEnv[k] ? fAtt * fEnv[k] + (1 - fAtt) * a : fRel * fEnv[k] + (1 - fRel) * a;
        }
        if (fCnt <= 0)
        {
            gains();
            fCnt = 16;
        }
        fCnt -= 1;

        const double dl = spl0, dr = spl1;
        double l, r;
        if (fChan == 0)
        {
            l = run(spl0, 0);
            r = run(spl1, 4);
        }
        else
        {
            double m = (spl0 + spl1) * 0.5, sd = (spl0 - spl1) * 0.5;
            if (fChan == 1)
                m = run(m, 0);
            else
                sd = run(sd, 0);
            l = m + sd;
            r = m - sd;
        }
        if (fDelta)
        {
            spl0 = (dl - l) * fOut;
            spl1 = (dr - r) * fOut;
        }
        else
        {
            spl0 = (dl + (l - dl) * fMix) * fOut;
            spl1 = (dr + (r - dr) * fMix) * fOut;
        }
    }

    // for the display: the gain reduction (dB) and the key level (amplitude) of band k
    double gr(int k) const { return fGr[k]; }
    double keyLevel(int k) const { return fEnv[k]; }

private:
    // sd_gains + sd_bells
    void gains()
    {
        for (int k = 0; k < kBands; ++k)
        {
            const double over = 20 * std::log10(std::max(fEnv[k], 0.000000001)) - fThr;
            fGr[k] = fOn[k] && over > 0 ? std::min(fCut, over) : 0;
        }
        for (int k = 0; k < kBands; ++k)
        {
            const double aa = std::pow(10.0, -fGr[k] * 0.43 / 40);
            const double a0 = 1 + fAL[k] / aa;
            double* c = fMC[k];
            c[0] = (1 + fAL[k] * aa) / a0;
            c[1] = -2 * fCW[k] / a0;
            c[2] = (1 - fAL[k] * aa) / a0;
            c[3] = c[1];
            c[4] = (1 - fAL[k] / aa) / a0;
        }
    }

    // sd_run: x through the 32 bells, state offset 0 (left, mid or side) or 4 (right)
    double run(double x, int ch)
    {
        for (int k = 0; k < kBands; ++k)
        {
            const double* c = fMC[k];
            double* s = fMS[k] + ch;
            const double y = c[0] * x + c[1] * s[0] + c[2] * s[1] - c[3] * s[2] - c[4] * s[3];
            s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
            x = y;
        }
        return x;
    }

    double fSr = 48000.0;
    double fKC[kBands][3] = {}, fKS[kBands][8] = {}, fEnv[kBands] = {}, fGr[kBands] = {};
    double fMC[kBands][5] = {}, fMS[kBands][8] = {}, fCW[kBands] = {}, fAL[kBands] = {};
    bool fOn[kBands] = {};
    double fCut = 12, fThr = -40, fAtt = 0, fRel = 0, fMix = 1, fOut = 1;
    int fChan = 0, fCnt = 0;
    bool fDelta = false;
};

} // namespace nullsduck
