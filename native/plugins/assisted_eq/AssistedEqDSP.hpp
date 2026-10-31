/*
 * NULL JSFX - Assisted EQ: the DSP of assisted_eq.jsfx in double, in the EEL2 order
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Learn listens with 29 third-octave band-passes in 50 ms windows (only windows above
 * -60 dBFS RMS count). After 15 s of signal, or when Learn is turned off after 0.5 s, it writes
 * its measurements into its own sliders 9-16 and sets Learn (slider 1) back to Off, as the
 * JSFX does in its code; the plugin then stores them as its parameter values (learned()).
 * The EQ itself is seven RBJ stages (HPF, low shelf, mud, honk, presence, air shelf, LPF)
 * worked out from the sliders and the rule tables (AssistedTargets.hpp) in update().
 * All buffers are members: nothing is allocated.
 */

#pragma once

#include "AssistedTargets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullassistedeq {

static constexpr int kNumValues = 16; // slider1..slider16 of assisted_eq.jsfx
static constexpr int kBands = 29;
static constexpr int kStages = 7;

class AssistedEqProcessor {
public:
    // s[i] is slider i+1 (the JSFX's doubles)
    double s[kNumValues] = { 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 300, 0, 0, 0, 20 };

    void setSampleRate(double sr)
    {
        fSr = sr;
        for (int k = 0; k < kBands; ++k)
            fF[k] = 1000 * std::pow(10.0, double(k - 16) / 10);
        for (int i = 0; i < kStages; ++i)
            for (int j = 0; j < 4; ++j)
                fSt[i][j] = 0;
        fOn = false;
        fPrev = 0;
        update();
    }

    // the JSFX's update(): the EQ from the sliders and the tables
    void update()
    {
        using namespace nullassisted;
        fSrc = int(s[1]);
        fTgt = int(s[2]);
        const bool lr = s[8] >= 0.5;
        const double amt = s[3] / 100;
        fMreg[0] = s[9]; fMreg[1] = s[10]; fMreg[2] = s[12]; fMreg[3] = s[13]; fMreg[4] = s[14];
        for (int i = 0; i < 5; ++i)
            fG[i] = amt * std::max(kEqMaxCut, std::min(kEqMaxBoost, kEqFlavor[fTgt][i] + (lr ? corr(i, fMreg[i]) : 0.0)));
        fG[0] += s[4];
        fG[2] += s[5];
        fG[4] += s[6];
        fHp = lr ? std::max(kEqSourceHpfLo[fSrc], std::min(kEqSourceHpfHi[fSrc], 0.7 * s[15])) : 0.0;
        if (fHp <= 20)
            fHp = 0;
        fHp = std::max(fHp, kEqTargetHpf[fTgt]);
        fLp = kEqTargetLpf[fTgt];
        fMf = lr ? s[11] : 300.0;
        set(0, fHp > 0 && amt > 0 ? 4 : 0, fHp, 0.7071, 0);
        set(1, 2, 100, 0.7071, fG[0]);
        set(2, 1, fMf, 1.2, fG[1]);
        set(3, 1, 1000, 1, fG[2]);
        set(4, 1, 3000, 0.8, fG[3]);
        set(5, 3, 10000, 0.7071, fG[4]);
        set(6, fLp > 0 && amt > 0 ? 5 : 0, fLp, 0.7071, 0);
        double sw = 0, sh = 0;
        for (int i = 0; i < kBands; ++i)
        {
            if (fF[i] < 0.45 * fSr)
            {
                const int r = i >= 4 && i <= 7 ? 0 : i >= 9 && i <= 13 ? 1 : i >= 15 && i <= 18 ? 2
                            : i >= 19 && i <= 23 ? 3 : i >= 25 ? 4 : -1;
                const double db = -4.5 * std::log(fF[i] / 1000) / std::log(2.0) + (lr && r >= 0 ? fMreg[r] : 0.0);
                const double w = std::pow(10.0, db / 10);
                sw += w;
                sh += w * mag2(fF[i]);
            }
        }
        fAuto = sh > 0 ? std::max(-12.0, std::min(12.0, -10 * std::log10(sh / sw))) : 0.0;
        fOut = std::pow(10.0, (s[7] + fAuto) / 20);
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
            double l = inL[i], r = inR[i];
            if (fOn)
            {
                const double m = (l + r) * 0.5;
                for (int k = 0; k < fNv; ++k)
                {
                    const double y = fC0[k] * m + fZ1[k];
                    fZ1[k] = fZ2[k] - fC1[k] * y;
                    fZ2[k] = -fC0[k] * m - fC2[k] * y;
                    fWa[k] += y * y;
                }
                fWe += m * m;
                fWn += 1;
                if (fWn >= fWin)
                {
                    if (fWe > 0.000001 * fWn)
                    {
                        for (int k = 0; k < fNv; ++k)
                            fAcc[k] += fWa[k];
                        fN += fWn;
                    }
                    for (int k = 0; k < kBands; ++k)
                        fWa[k] = 0;
                    fWe = 0;
                    fWn = 0;
                    if (fN >= fNeed)
                    {
                        finish();
                        changed = true;
                    }
                }
            }
            for (int st = 0; st < kStages; ++st)
            {
                const double* p = fCf[st];
                double* q = fSt[st];
                double y = p[0] * l + q[0];
                q[0] = p[1] * l - p[3] * y + q[1];
                q[1] = p[2] * l - p[4] * y;
                l = y;
                y = p[0] * r + q[2];
                q[2] = p[1] * r - p[3] * y + q[3];
                q[3] = p[2] * r - p[4] * y;
                r = y;
            }
            l *= fOut;
            r *= fOut;
            outL[i] = float(l);
            outR[i] = float(r);
        }
        return changed;
    }

    // |H|^2 of the whole EQ at frequency f
    double mag2(double f) const
    {
        const double w = 2 * M_PI * f / fSr;
        const double c1 = std::cos(w), c2 = std::cos(2 * w);
        double m = 1;
        for (int st = 0; st < kStages; ++st)
        {
            const double* p = fCf[st];
            const double n = p[0] * p[0] + p[1] * p[1] + p[2] * p[2] + 2 * (p[0] * p[1] + p[1] * p[2]) * c1 + 2 * p[0] * p[2] * c2;
            const double d = 1 + p[3] * p[3] + p[4] * p[4] + 2 * (p[3] + p[3] * p[4]) * c1 + 2 * p[4] * c2;
            m *= n / d;
        }
        return m;
    }

    // for the interface
    bool learning() const { return fOn; }
    double progress() const { return fNeed > 0 ? std::min(1.0, fN / fNeed) : 0.0; }
    double heard() const { return fSr > 0 ? fN / fSr : 0.0; }
    double gain(int i) const { return fG[i]; }
    double mudFreq() const { return fMf; }
    double highPass() const { return fHp; }
    double autoLevel() const { return fAuto; }
    double outGain() const { return fOut; }

private:
    void band(int i)
    {
        const double w = 2 * M_PI * fF[i] / fSr;
        const double x = std::log(2.0) / 6 * w / std::sin(w);
        const double al = std::sin(w) * (std::exp(x) - std::exp(-x)) / 2;
        fC0[i] = al / (1 + al);
        fC1[i] = -2 * std::cos(w) / (1 + al);
        fC2[i] = (1 - al) / (1 + al);
    }

    void start()
    {
        fNv = 0;
        for (int i = 0; i < kBands; ++i)
        {
            if (fF[i] < 0.45 * fSr)
            {
                band(i);
                fNv = i + 1;
            }
        }
        for (int i = 0; i < kBands; ++i)
            fZ1[i] = fZ2[i] = fWa[i] = fAcc[i] = 0;
        fWe = 0;
        fWn = 0;
        fN = 0;
        fWin = std::floor(0.05 * fSr);
        fNeed = std::floor(15 * fSr);
        fOn = true;
    }

    double region(int i0, int i1) const
    {
        double sum = 0, n = 0;
        for (int i = i0; i <= i1; ++i)
        {
            if (i < fNv)
            {
                sum += fDev[i];
                n += 1;
            }
        }
        return n > 0 ? sum / n : 0.0;
    }

    static double round1(double v) { return std::floor(v * 10 + 0.5) / 10; }
    static double clamp24(double v) { return std::max(-24.0, std::min(24.0, v)); }

    void finish()
    {
        double mx = -1000;
        for (int i = 0; i < fNv; ++i)
        {
            fDev[i] = 10 * std::log10(fAcc[i] / fN + 0.000000000000000000000000000001);
            mx = std::max(mx, fDev[i]);
        }
        int lo = 0;
        for (int i = fNv - 1; i >= 0; --i)
            if (fDev[i] > mx - 30)
                lo = i;
        for (int i = 0; i < fNv; ++i)
            fDev[i] += 4.5 * std::log(fF[i] / 1000) / std::log(2.0);
        double sum = 0, n = 0;
        for (int i = 13; i < 23; ++i)
        {
            if (i < fNv)
            {
                sum += fDev[i];
                n += 1;
            }
        }
        sum = n > 0 ? sum / n : 0.0;
        for (int i = 0; i < fNv; ++i)
            fDev[i] -= sum;
        s[9] = round1(clamp24(region(4, 7)));
        int pk = 9;
        for (int i = 10; i < 14; ++i)
            if (i < fNv && fDev[i] > fDev[pk])
                pk = i;
        s[10] = round1(clamp24(region(std::max(9, pk - 1), std::min(13, pk + 1))));
        s[11] = std::max(200.0, std::min(500.0, std::floor(fF[pk] + 0.5)));
        s[12] = round1(clamp24(region(15, 18)));
        s[13] = round1(clamp24(region(19, 23)));
        s[14] = round1(clamp24(region(25, 28)));
        s[15] = std::max(20.0, std::min(500.0, std::floor(fF[lo] + 0.5)));
        s[8] = 1;
        s[0] = 0;
        fOn = false;
        fPrev = 0;
        update();
    }

    double corr(int r, double d) const
    {
        using namespace nullassisted;
        const double x = d - kEqSourceRef[fSrc][r];
        const double sg = x > 0 ? 1.0 : x < 0 ? -1.0 : 0.0;
        double c = std::fabs(x) > kEqDeadband ? -kEqCorrection * (x - sg * kEqDeadband) : 0.0;
        c = std::max(kEqMaxCut, std::min(kEqMaxBoost, c));
        if (r == 1)
            c = std::min(0.0, c);
        return c;
    }

    // RBJ coefficients of stage st: type 0 off, 1 peak, 2 low shelf, 3 high shelf, 4 HPF, 5 LPF
    void set(int st, int type, double f, double q, double g)
    {
        double* p = fCf[st];
        if (type == 0)
        {
            p[0] = 1; p[1] = 0; p[2] = 0; p[3] = 0; p[4] = 0;
            return;
        }
        const double a = std::pow(10.0, g / 40);
        const double w = 2 * M_PI * std::min(f, 0.45 * fSr) / fSr;
        const double cs = std::cos(w);
        const double al = std::sin(w) / (2 * q);
        const double sa = 2 * std::sqrt(a) * al;
        double b0, b1, b2, a0, a1, a2;
        if (type == 1)
        {
            b0 = 1 + al * a; b1 = -2 * cs; b2 = 1 - al * a;
            a0 = 1 + al / a; a1 = -2 * cs; a2 = 1 - al / a;
        }
        else if (type == 2)
        {
            b0 = a * ((a + 1) - (a - 1) * cs + sa);
            b1 = 2 * a * ((a - 1) - (a + 1) * cs);
            b2 = a * ((a + 1) - (a - 1) * cs - sa);
            a0 = (a + 1) + (a - 1) * cs + sa;
            a1 = -2 * ((a - 1) + (a + 1) * cs);
            a2 = (a + 1) + (a - 1) * cs - sa;
        }
        else if (type == 3)
        {
            b0 = a * ((a + 1) + (a - 1) * cs + sa);
            b1 = -2 * a * ((a - 1) + (a + 1) * cs);
            b2 = a * ((a + 1) + (a - 1) * cs - sa);
            a0 = (a + 1) - (a - 1) * cs + sa;
            a1 = 2 * ((a - 1) - (a + 1) * cs);
            a2 = (a + 1) - (a - 1) * cs - sa;
        }
        else if (type == 4)
        {
            b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2;
            a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al;
        }
        else
        {
            b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2;
            a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al;
        }
        p[0] = b0 / a0; p[1] = b1 / a0; p[2] = b2 / a0; p[3] = a1 / a0; p[4] = a2 / a0;
    }

    double fSr = 48000;
    double fF[kBands] = {}, fC0[kBands] = {}, fC1[kBands] = {}, fC2[kBands] = {};
    double fZ1[kBands] = {}, fZ2[kBands] = {}, fWa[kBands] = {}, fAcc[kBands] = {}, fDev[kBands] = {};
    int fNv = 0;
    double fWe = 0, fWn = 0, fN = 0, fWin = 2400, fNeed = 720000;
    bool fOn = false;
    double fPrev = 0;
    double fCf[kStages][5] = {}, fSt[kStages][4] = {};
    double fG[5] = {}, fMreg[5] = {};
    double fHp = 0, fLp = 0, fMf = 300, fAuto = 0, fOut = 1;
    int fSrc = 0, fTgt = 0;
};

} // namespace nullassistedeq
