/*
 * NULL JSFX - Assisted Reverb: the DSP of assisted_reverb.jsfx in double, in the EEL2 order
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Learn listens in 50 ms windows (only windows above -60 dBFS RMS count): a level histogram,
 * the time between onsets and the brightness (power above 4 kHz against the whole). After 15 s
 * of signal, or when Learn is turned off after 0.5 s, it writes its measurements into its own
 * sliders 9-13 and sets Learn (slider 1) back to Off, as the JSFX does in its code. The reverb
 * (pre-delay, 8 damped combs and 4 all-passes per side, wet low cut and high cut, ducking) is
 * worked out from the sliders and the rule tables (AssistedTargets.hpp) in update().
 * The lines are sized in setSampleRate; process allocates nothing.
 */

#pragma once

#include "AssistedTargets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullassistedreverb {

static constexpr int kNumValues = 13; // slider1..slider13 of assisted_reverb.jsfx
static constexpr int kLevelBins = 800;

class AssistedReverbProcessor {
public:
    // s[i] is slider i+1 (the JSFX's doubles)
    double s[kNumValues] = { 0, 0, 0, 100, 0, 0, 0, 0, 0, -18, -24, 0, -18 };

    // the JSFX's @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSc = sr / 44100;
        fBlen = int(std::ceil((1617 + 23) * 1.1 * fSc)) + 4;
        fAlen = int(std::ceil((556 + 23) * 1.1 * fSc)) + 4;
        fPdn = int(std::ceil(0.061 * sr)) + 2;
        fComb.assign(size_t(16) * fBlen, 0.0);
        fAll.assign(size_t(8) * fAlen, 0.0);
        fPd.assign(size_t(fPdn), 0.0);
        for (int i = 0; i < 16; ++i)
            fCi[i] = fClen[i] = fCd[i] = fCfb[i] = 0;
        for (int i = 0; i < 8; ++i)
            fAi[i] = fAlenI[i] = 0;
        fPdw = 0;
        fOn = false;
        fPrev = 0;
        fEnv = 0;
        fDuckg = 1;
        fLc1l = fLc2l = fLc1r = fLc2r = 0;
        fHc1l = fHc2l = fHc1r = fHc2r = 0;
        fEf = 1 - std::exp(-1 / (0.005 * sr));
        fEs = 1 - std::exp(-1 / (0.1 * sr));
        fDka = 1 - std::exp(-1 / (0.01 * sr));
        fDkr = 1 - std::exp(-1 / (0.25 * sr));
        const double w = 2 * M_PI * std::min(4000.0, 0.45 * sr) / sr;
        const double a = std::sin(w) / (2 * 0.7071067811865476);
        const double cs = std::cos(w);
        fH0 = (1 + cs) / 2 / (1 + a);
        fH1 = -(1 + cs) / (1 + a);
        fH3 = -2 * cs / (1 + a);
        fH4 = (1 - a) / (1 + a);
        update();
    }

    // the JSFX's update(): the reverb from the sliders and the tables
    void update()
    {
        using namespace nullassisted;
        fSrc = std::max(0, std::min(kSources - 1, int(s[1])));
        fTgt = std::max(0, std::min(kTargets - 1, int(s[2])));
        const bool lr = s[8] >= 0.5;
        const double amt = s[3] / 100;
        fDec = kRvDecay[fTgt] * kRvDecayMul[fSrc];
        if (lr && s[11] > 0)
            fDec = std::min(fDec, std::max(kRvDecayMin, kRvDecayOnsets * s[11] / 1000));
        fDec = std::max(0.2, std::min(10.0, fDec * std::pow(2.0, s[4])));
        fPdms = lr && s[11] > 0 ? std::max(kRvPredelayMin, std::min(kRvPredelayMax, s[11] * kRvPredelayShare * kRvPredelayMul[fTgt]))
                                : kRvPredelayNone * kRvPredelayMul[fTgt];
        fPdms = std::max(0.0, std::min(60.0, fPdms));
        fPdl = int(std::floor(fPdms * fSr / 1000 + 0.5));
        fLcf = std::max(kRvLowCut[fSrc], kRvLowCutTarget[fTgt]);
        fHcf = kRvHighCut[fTgt] * (lr ? std::pow(2.0, (s[12] - kRvBrightRef) / kRvBrightDb) : 1.0);
        fHcf = std::max(kRvHighCutMin, std::min(kRvHighCutMax, fHcf)) * std::pow(2.0, s[5]);
        fHcf = std::max(500.0, std::min(20000.0, fHcf));
        bq(fLo, fLcf, true);
        bq(fHi, fHcf, false);
        fDamp = std::exp(-2 * M_PI * std::min(fHcf, 0.45 * fSr) / fSr);
        fDampa = 1 - fDamp;
        static const double kCl[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const double kAl[4] = { 556, 441, 341, 225 };
        const double k = kRvSize[fTgt] * fSc;
        for (int i = 0; i < 16; ++i)
        {
            const double sp = i >= 8 ? 23 : 0;
            const double len = std::max(1.0, std::min(double(fBlen - 1), std::floor((kCl[i % 8] + sp) * k)));
            fClen[i] = len;
            if (fCi[i] >= len)
                fCi[i] = 0;
            fCfb[i] = std::pow(10.0, -3 * len / (fDec * fSr));
        }
        for (int i = 0; i < 8; ++i)
        {
            const double sp = i >= 4 ? 23 : 0;
            const double len = std::max(1.0, std::min(double(fAlen - 1), std::floor((kAl[i % 4] + sp) * k)));
            fAlenI[i] = len;
            if (fAi[i] >= len)
                fAi[i] = 0;
        }
        fWet = kRvMix[fSrc] * kRvMixMul[fTgt] * amt * std::pow(10.0, s[6] / 20);
        fDuck = kRvDuck[fSrc] * kRvDuckMul[fTgt] * std::min(1.0, amt);
        fDlo = s[10];
        fDrng = std::max(3.0, s[9] - s[10]);
        fOut = std::pow(10.0, s[7] / 20);
    }

    // the JSFX's @block: Learn turned on starts, turned off early finishes or gives up.
    // Returns true when the sliders changed (Learn finished).
    bool block()
    {
        bool changed = false;
        if (s[0] >= 0.5 && fPrev < 0.5)
            start();
        if (s[0] < 0.5 && fOn)
        {
            if (fN >= 0.5 * fSr)
            {
                finish();
                changed = true;
            }
            else
                fOn = false;
        }
        fPrev = s[0] >= 0.5 ? 1 : 0;
        return changed;
    }

    // the JSFX's @sample over a block; returns true when Learn finished in it
    bool process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
    {
        bool changed = false;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            const double x = std::max(std::fabs(l), std::fabs(r));
            if (fOn && listen(l, r, x))
                changed = true;
            fEnv += (x - fEnv) * (x > fEnv ? fDka : fDkr);
            if (fDuck > 0)
            {
                const double dt = std::max(0.0, std::min(1.0, (20 * std::log10(std::max(fEnv, 0.000001)) - fDlo) / fDrng));
                fDuckg = std::pow(10.0, -fDuck * dt / 20);
            }
            else
                fDuckg = 1;

            fPd[size_t(fPdw)] = (l + r) * 0.5;
            int pr = fPdw - fPdl;
            if (pr < 0)
                pr += fPdn;
            const double in0 = fPd[size_t(pr)] * 0.07;
            if (++fPdw >= fPdn)
                fPdw = 0;

            double wl = 0, wr = 0;
            for (int c = 0; c < 16; ++c)
            {
                double* b = fComb.data() + size_t(c) * fBlen;
                const int idx = int(fCi[c]);
                const double y = b[idx];
                fCd[c] = y * fDampa + fCd[c] * fDamp;
                b[idx] = in0 + fCd[c] * fCfb[c];
                fCi[c] += 1;
                if (fCi[c] >= fClen[c])
                    fCi[c] = 0;
                if (c < 8)
                    wl += y;
                else
                    wr += y;
            }
            for (int a = 0; a < 8; ++a)
            {
                double* b = fAll.data() + size_t(a) * fAlen;
                const int idx = int(fAi[a]);
                const double y = b[idx];
                if (a < 4)
                {
                    b[idx] = wl + y * 0.5;
                    wl = y - wl;
                }
                else
                {
                    b[idx] = wr + y * 0.5;
                    wr = y - wr;
                }
                fAi[a] += 1;
                if (fAi[a] >= fAlenI[a])
                    fAi[a] = 0;
            }

            double y = fLo.b0 * wl + fLc1l;
            fLc1l = fLo.b1 * wl - fLo.a1 * y + fLc2l;
            fLc2l = fLo.b2 * wl - fLo.a2 * y;
            wl = y;
            y = fLo.b0 * wr + fLc1r;
            fLc1r = fLo.b1 * wr - fLo.a1 * y + fLc2r;
            fLc2r = fLo.b2 * wr - fLo.a2 * y;
            wr = y;
            y = fHi.b0 * wl + fHc1l;
            fHc1l = fHi.b1 * wl - fHi.a1 * y + fHc2l;
            fHc2l = fHi.b2 * wl - fHi.a2 * y;
            wl = y;
            y = fHi.b0 * wr + fHc1r;
            fHc1r = fHi.b1 * wr - fHi.a1 * y + fHc2r;
            fHc2r = fHi.b2 * wr - fHi.a2 * y;
            wr = y;

            const double g = fWet * fDuckg;
            outL[i] = float((l + wl * g) * fOut);
            outR[i] = float((r + wr * g) * fOut);
        }
        return changed;
    }

    // for the interface
    bool learning() const { return fOn; }
    double progress() const { return fNeed > 0 ? std::min(1.0, fN / fNeed) : 0.0; }
    double heard() const { return fSr > 0 ? fN / fSr : 0.0; }
    double duckDb() const { return -20 * std::log10(fDuckg); }
    double decay() const { return fDec; }
    double predelayMs() const { return fPdms; }
    double lowCut() const { return fLcf; }
    double highCut() const { return fHcf; }
    double wet() const { return fWet; }
    double duck() const { return fDuck; }

private:
    struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    void bq(Bq& q, double f, bool hp) const
    {
        const double w = 2 * M_PI * std::min(f, 0.45 * fSr) / fSr;
        const double a = std::sin(w) / (2 * 0.7071067811865476);
        const double c = std::cos(w);
        const double a0 = 1 + a;
        if (hp)
        {
            q.b0 = (1 + c) / 2 / a0;
            q.b1 = -(1 + c) / a0;
        }
        else
        {
            q.b0 = (1 - c) / 2 / a0;
            q.b1 = (1 - c) / a0;
        }
        q.b2 = q.b0;
        q.a1 = -2 * c / a0;
        q.a2 = (1 - a) / a0;
    }

    void start()
    {
        std::fill(fLh, fLh + kLevelBins, 0.0);
        fWin = std::floor(0.05 * fSr);
        fNeed = std::floor(15 * fSr);
        fWn = fWp = fWh = fWm = 0;
        fN = 0;
        fHs = fPs = 0;
        fZ1 = fZ2 = 0;
        fFf = fFs = 0;
        fT = 0; fLast = -1; fIs = 0; fIn = 0;
        fOn = true;
    }

    static double round1(double v) { return std::floor(v * 10 + 0.5) / 10; }

    double pct(double p, double total) const
    {
        double c = 0;
        int r = -1;
        for (int i = 0; i < kLevelBins; ++i)
        {
            c += fLh[i];
            if (r < 0 && c >= p * total)
                r = i;
        }
        return -80 + (std::max(0, r) + 0.5) / 10;
    }

    // one sample of the JSFX's learn analysis; returns true when Learn finished
    bool listen(double l, double r, double x)
    {
        const double m = (l + r) * 0.5;
        fWp += (l * l + r * r) * 0.5;
        const double y = fH0 * m + fZ1;
        fZ1 = fH1 * m - fH3 * y + fZ2;
        fZ2 = fH0 * m - fH4 * y;
        fWh += y * y;
        fWm += m * m;
        fWn += 1;
        fFf += (x - fFf) * fEf;
        fFs += (x - fFs) * fEs;
        if (fFf > 0.003 && fFf > 2 * fFs && (fLast < 0 || fT - fLast >= 0.05 * fSr))
        {
            if (fLast >= 0 && fT - fLast <= 2 * fSr)
            {
                fIs += fT - fLast;
                fIn += 1;
            }
            fLast = fT;
        }
        fT += 1;
        if (fWn >= fWin)
        {
            if (fWp > 0.000001 * fWn)
            {
                fLh[int(std::max(0.0, std::min(double(kLevelBins - 1), std::floor((10 * std::log10(fWp / fWn) + 80) * 10))))] += 1;
                fHs += fWh;
                fPs += fWm;
                fN += fWn;
            }
            fWp = fWh = fWm = 0;
            fWn = 0;
            if (fN >= fNeed)
            {
                finish();
                return true;
            }
        }
        return false;
    }

    void finish()
    {
        double total = 0;
        for (int i = 0; i < kLevelBins; ++i)
            total += fLh[i];
        s[9] = round1(std::max(-80.0, std::min(0.0, pct(0.9, total))));
        s[10] = round1(std::max(-80.0, std::min(0.0, pct(0.5, total))));
        s[11] = fIn >= 2 ? std::max(0.0, std::min(2000.0, std::floor(fIs / fIn * 1000 / fSr + 0.5))) : 0.0;
        s[12] = round1(std::max(-60.0, std::min(0.0, fPs > 0 ? 10 * std::log10(fHs / fPs + 0.000000000001) : -60.0)));
        s[8] = 1;
        s[0] = 0;
        fOn = false;
        fPrev = 0;
        update();
    }

    double fSr = 48000, fSc = 48000.0 / 44100;
    int fBlen = 1, fAlen = 1, fPdn = 1;
    std::vector<double> fComb, fAll, fPd;
    double fCi[16] = {}, fClen[16] = {}, fCd[16] = {}, fCfb[16] = {};
    double fAi[8] = {}, fAlenI[8] = {};
    int fPdw = 0, fPdl = 0;
    Bq fLo, fHi;
    double fLc1l = 0, fLc2l = 0, fLc1r = 0, fLc2r = 0, fHc1l = 0, fHc2l = 0, fHc1r = 0, fHc2r = 0;
    double fEf = 0, fEs = 0, fDka = 0, fDkr = 0, fH0 = 1, fH1 = 0, fH3 = 0, fH4 = 0;
    // learn state
    bool fOn = false;
    double fPrev = 0;
    double fLh[kLevelBins] = {};
    double fWin = 2400, fNeed = 720000;
    double fWn = 0, fWp = 0, fWh = 0, fWm = 0, fN = 0, fHs = 0, fPs = 0, fZ1 = 0, fZ2 = 0;
    double fFf = 0, fFs = 0, fT = 0, fLast = -1, fIs = 0, fIn = 0;
    // the reverb
    int fSrc = 0, fTgt = 0;
    double fDec = 1, fPdms = 0, fLcf = 20, fHcf = 20000, fDamp = 0, fDampa = 1;
    double fWet = 0, fDuck = 0, fDlo = -24, fDrng = 6, fOut = 1, fEnv = 0, fDuckg = 1;
};

} // namespace nullassistedreverb
