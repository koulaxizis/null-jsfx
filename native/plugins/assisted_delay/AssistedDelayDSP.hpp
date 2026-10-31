/*
 * NULL JSFX - Assisted Delay: the DSP of assisted_delay.jsfx in double, in the EEL2 order
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Learn listens as Assisted Reverb does (levels, onsets, brightness) and writes its sliders
 * 9-13, then sets Learn back to Off. The delay (the target's note at the host tempo or a
 * slapback, stereo or ping-pong, one-pole low cut and high cut in the feedback loop, ducking)
 * is worked out from the sliders and the rule tables (AssistedTargets.hpp) in update().
 * The lines (3.05 s per side) are sized in setSampleRate; process allocates nothing.
 */

#pragma once

#include "AssistedTargets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullassisteddelay {

static constexpr int kNumValues = 13; // slider1..slider13 of assisted_delay.jsfx
static constexpr int kLevelBins = 800;

class AssistedDelayProcessor {
public:
    // s[i] is slider i+1 (the JSFX's doubles)
    double s[kNumValues] = { 0, 0, 0, 100, 0, 0, 0, 0, 0, -18, -24, 0, -18 };

    // the JSFX's @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(3.05 * sr));
        fBl.assign(size_t(fSize), 0.0);
        fBr.assign(size_t(fSize), 0.0);
        fWp = 0;
        fD = 1;
        fOn = false;
        fPrev = 0;
        fEnv = 0;
        fDuckg = 1;
        fLcl = fLcr = fHcl = fHcr = 0;
        fBpm = 120;
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

    // the JSFX's update(): the delay from the sliders and the tables
    void update()
    {
        using namespace nullassisted;
        fSrc = std::max(0, std::min(kSources - 1, int(s[1])));
        fTgt = std::max(0, std::min(kTargets - 1, int(s[2])));
        const bool lr = s[8] >= 0.5;
        const double amt = s[3] / 100;
        time();
        fPp = kDlPingPong[fTgt] >= 0.5;
        double rep = kDlRepeats[fTgt] * std::pow(2.0, s[4]);
        if (lr && s[11] > 0 && s[11] < fMs)
            rep *= std::max(0.5, s[11] / fMs);
        fRep = std::max(0.25, rep);
        fFb = std::min(0.9, std::pow(10.0, -30 / fRep / 20));
        fLcf = kDlLowCut[fSrc];
        fHcf = kDlHighCut[fTgt] * (lr ? std::pow(2.0, (s[12] - kDlBrightRef) / kDlBrightDb) : 1.0);
        fHcf = std::max(kDlHighCutMin, std::min(kDlHighCutMax, fHcf)) * std::pow(2.0, s[5]);
        fHcf = std::max(500.0, std::min(20000.0, fHcf));
        fLca = 1 - std::exp(-2 * M_PI * std::min(fLcf, 0.45 * fSr) / fSr);
        fHca = 1 - std::exp(-2 * M_PI * std::min(fHcf, 0.45 * fSr) / fSr);
        fWet = kDlMix[fSrc] * kDlMixMul[fTgt] * amt * std::pow(10.0, s[6] / 20);
        fDuck = kDlDuck[fSrc] * kDlDuckMul[fTgt] * std::min(1.0, amt);
        fDlo = s[10];
        fDrng = std::max(3.0, s[9] - s[10]);
        fOut = std::pow(10.0, s[7] / 20);
    }

    // the JSFX's @block: the host tempo (0 or less: none, 120 bpm), then Learn turned on
    // starts, turned off early finishes or gives up. Returns true when Learn finished.
    bool block(double tempo)
    {
        const double nb = tempo > 0 ? tempo : 120.0;
        if (nb != fBpm)
        {
            fBpm = nb;
            update();
        }
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

            int rp = fWp - fD;
            if (rp < 0)
                rp += fSize;
            double el = fBl[size_t(rp)];
            double er = fBr[size_t(rp)];
            fLcl += fLca * (el - fLcl); el -= fLcl;
            fLcr += fLca * (er - fLcr); er -= fLcr;
            fHcl += fHca * (el - fHcl); el = fHcl;
            fHcr += fHca * (er - fHcr); er = fHcr;
            if (fPp)
            {
                fBl[size_t(fWp)] = (l + r) * 0.5 + er * fFb;
                fBr[size_t(fWp)] = el * fFb;
            }
            else
            {
                fBl[size_t(fWp)] = l + el * fFb;
                fBr[size_t(fWp)] = r + er * fFb;
            }
            if (++fWp >= fSize)
                fWp = 0;

            const double g = fWet * fDuckg;
            outL[i] = float((l + el * g) * fOut);
            outR[i] = float((r + er * g) * fOut);
        }
        return changed;
    }

    // for the interface
    bool learning() const { return fOn; }
    double progress() const { return fNeed > 0 ? std::min(1.0, fN / fNeed) : 0.0; }
    double heard() const { return fSr > 0 ? fN / fSr : 0.0; }
    double duckDb() const { return -20 * std::log10(fDuckg); }
    double timeMs() const { return fMs; }
    bool pingPong() const { return fPp; }
    double repeats() const { return fRep; }
    double feedback() const { return fFb; }
    double lowCut() const { return fLcf; }
    double highCut() const { return fHcf; }
    double wet() const { return fWet; }
    double duck() const { return fDuck; }

private:
    void time()
    {
        using namespace nullassisted;
        fMs = kDlNote[fTgt] > 0 ? 60000 / fBpm * kDlNote[fTgt] : kDlSlapMs;
        fMs = std::max(1.0, std::min(3000.0, fMs));
        fD = int(std::max(1.0, std::min(double(fSize - 1), std::floor(fMs * fSr / 1000 + 0.5))));
    }

    void start()
    {
        std::fill(fLh, fLh + kLevelBins, 0.0);
        fWin = std::floor(0.05 * fSr);
        fNeed = std::floor(15 * fSr);
        fWn = fWq = fWh = fWm = 0;
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

    bool listen(double l, double r, double x)
    {
        const double m = (l + r) * 0.5;
        fWq += (l * l + r * r) * 0.5;
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
            if (fWq > 0.000001 * fWn)
            {
                fLh[int(std::max(0.0, std::min(double(kLevelBins - 1), std::floor((10 * std::log10(fWq / fWn) + 80) * 10))))] += 1;
                fHs += fWh;
                fPs += fWm;
                fN += fWn;
            }
            fWq = fWh = fWm = 0;
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

    double fSr = 48000;
    int fSize = 1, fWp = 0, fD = 1;
    std::vector<double> fBl, fBr;
    double fLcl = 0, fLcr = 0, fHcl = 0, fHcr = 0;
    double fEf = 0, fEs = 0, fDka = 0, fDkr = 0, fH0 = 1, fH1 = 0, fH3 = 0, fH4 = 0;
    double fBpm = 120;
    // learn state
    bool fOn = false;
    double fPrev = 0;
    double fLh[kLevelBins] = {};
    double fWin = 2400, fNeed = 720000;
    double fWn = 0, fWq = 0, fWh = 0, fWm = 0, fN = 0, fHs = 0, fPs = 0, fZ1 = 0, fZ2 = 0;
    double fFf = 0, fFs = 0, fT = 0, fLast = -1, fIs = 0, fIn = 0;
    // the delay
    int fSrc = 0, fTgt = 0;
    bool fPp = false;
    double fMs = 500, fRep = 3, fFb = 0, fLcf = 20, fHcf = 20000, fLca = 0, fHca = 1;
    double fWet = 0, fDuck = 0, fDlo = -24, fDrng = 6, fOut = 1, fEnv = 0, fDuckg = 1;
};

} // namespace nullassisteddelay
