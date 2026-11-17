/*
 * NULL JSFX - Wind, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/wind.jsfx, computed in double in the EEL2 order: one voice (the
 * latest key held, gliding between keys held together) through one of four digital waveguides
 * after STK: flute (a bore of two periods, a jet a quarter of it, cubic jet), clarinet (a bore of
 * half a period, reed table), saxophone (one period in two delays, the reed between), brass (a
 * bore and a lip resonator, tuned per note by a table). The breath comes from the velocity or a
 * controller (CC 1, CC 2, aftertouch, CC 11), with attack, release, vibrato after a delay and air
 * noise by an exact Park-Miller step. The two delay lines are allocated in setSampleRate, never in
 * process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nullwind {

static constexpr int kMaxEvents = 512;

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

// brass: cents the bore is made longer by, per note from E2 (40) every 4 semitones to E6 (88)
static const double kBT[13] = { 115.5, 99.7, 86.4, 76.3, 68.6, 35.8, 18.3, 16.8, 19.6, 35.4, 41.1, 33.3, 27.6 };

// each model's range (notes outside sound an octave in)
inline void modelRange(int model, int& lo, int& hi)
{
    static const int kLo[4] = { 60, 50, 49, 40 }, kHi[4] = { 96, 91, 81, 88 };
    lo = kLo[model];
    hi = kHi[model];
}

class Wind {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fDL = int(std::ceil(sr * 0.06)) + 16;
        fA.assign(size_t(fDL), 0.0);
        fB.assign(size_t(fDL), 0.0);
        std::memset(fKH, 0, sizeof(fKH));
        fSeed = 12345.0;
        fHeld = 0.0; fKorder = 0.0; fSus = false; fGate = false;
        fNcur = -1.0; fNtgt = 60.0; fLev = 0.0; fEnv = 0.0; fBr = 0.0;
        fBendv = 0.0; fCc1 = fCc2 = fCc11 = fAt = 0.0;
        fTn = 0.0; fVph = 0.0; fNz = fNz1 = 0.0;
        fWa = fWb = 0;
        fModelWas = -1;
        fToneY = 0.0;
        fLp = fDcX = fDcY = fZ1 = fLy1 = fLy2 = 0.0;
    }

    // @slider (values as the JSFX sliders hold them)
    void setParams(const double* v)
    {
        fModel = int(std::max(0.0, std::min(3.0, std::floor(v[0] + 0.5))));
        fSrc = int(std::max(0.0, std::min(4.0, std::floor(v[1] + 0.5))));
        fAmt = v[2] / 100.0;
        fNf = v[3] / 50.0;
        fToneA = std::exp(-2.0 * M_PI * std::min(500.0 * std::pow(40.0, v[4] / 100.0), 0.45 * fSr) / fSr);
        fVib = v[5] / 40.0;
        fVrate = v[6];
        fVdel = v[7] * 0.001 * fSr;
        fVfade = 0.3 * fSr;
        fKa = 1.0 - std::exp(-4.6 / (v[8] * 0.001 * fSr));
        fKr = 1.0 - std::exp(-4.6 / (v[9] * 0.001 * fSr));
        fKg = v[10] > 0.0 ? 1.0 - std::exp(-4.6 / (v[10] * 0.001 * fSr)) : 1.0;
        fKsm = 1.0 - std::exp(-1000.0 / (10.0 * fSr));
        fVol = std::pow(10.0, v[11] / 20.0);
        modelRange(fModel, fNlo, fNhi);
        if (fModel != fModelWas)
        {
            resetModel();
            if (fNcur >= 0.0)
            {
                fNcur = fold(fNcur);
                fNtgt = fold(fNtgt);
            }
        }
        fModelWas = fModel;
        fLipR = std::exp(-M_PI * 45.9 / fSr);
        fBofs = 0.0000625 * fSr;
        fDcR = std::exp(-2.0 * M_PI * 38.3 / fSr);
    }

    void beginBlock(const MidiEv* ev, uint32_t count)
    {
        fMqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + fMqN, fMq);
        fMqI = 0;
        fSi = 0.0;
    }

    void process(float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double y = sample();
            outL[n] = float(y);
            outR[n] = float(y);
        }
    }

    // for the display: the note sounding (-1 for none) and the breath
    double noteNow() const { return fNcur >= 0.0 && fEnv > 0.001 ? std::floor(fNcur + 0.5) : -1.0; }
    double breathNow() const { return fBr; }

private:
    double noise()
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 1073741823.5 - 1.0;
    }

    double rd(const std::vector<double>& b, int w, double d) const
    {
        double p = w - d;
        if (p < 0.0)
            p += fDL;
        const double i = std::floor(p);
        const double f = p - i;
        int j = int(i) + 1;
        if (j >= fDL)
            j = 0;
        return b[size_t(i)] + (b[size_t(j)] - b[size_t(i)]) * f;
    }

    void resetModel()
    {
        std::fill(fA.begin(), fA.end(), 0.0);
        std::fill(fB.begin(), fB.end(), 0.0);
        fWa = fWb = 0;
        fLp = 0.0; fDcX = fDcY = 0.0; fZ1 = 0.0;
        fLy1 = fLy2 = 0.0;
    }

    double fold(double n) const
    {
        while (n < fNlo)
            n += 12.0;
        while (n > fNhi)
            n -= 12.0;
        return n;
    }

    void keyOn(int n, double v)
    {
        fKorder += 1.0;
        fKH[n] = fKorder;
        fHeld += 1.0;
        fNtgt = fold(n);
        if (fSrc == 0)
            fLev = v;
        if (!fGate || fNcur < 0.0)
        {
            fNcur = fNtgt;
            fTn = 0.0;
        }
        fGate = true;
    }

    void keyOff(int n)
    {
        if (fKH[n] > 0.0)
        {
            fKH[n] = 0.0;
            fHeld -= 1.0;
        }
        int best = -1;
        double bo = 0.0;
        for (int i = 0; i < 128; ++i)
            if (fKH[i] > bo)
            {
                bo = fKH[i];
                best = i;
            }
        if (best >= 0)
            fNtgt = fold(best);
        else if (!fSus)
            fGate = false;
    }

    void allOff()
    {
        std::memset(fKH, 0, sizeof(fKH));
        fHeld = 0.0;
        fSus = false;
        fGate = false;
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        const int n = int(d1) & 127;
        if (t == 0x90 && d2 > 0.0)
            keyOn(n, d2 / 127.0);
        else if (t == 0x80 || t == 0x90)
            keyOff(n);
        else if (t == 0xE0)
            fBendv = (d2 * 128.0 + d1 - 8192.0) / 8192.0;
        else if (t == 0xD0)
            fAt = d1 / 127.0;
        else if (t == 0xA0)
        {
            if (fKH[n] > 0.0)
                fAt = d2 / 127.0;
        }
        else if (t == 0xB0)
        {
            if (d1 == 1.0)
                fCc1 = d2 / 127.0;
            else if (d1 == 2.0)
                fCc2 = d2 / 127.0;
            else if (d1 == 11.0)
                fCc11 = d2 / 127.0;
            else if (d1 == 64.0)
            {
                if (d2 >= 64.0)
                    fSus = true;
                else
                {
                    fSus = false;
                    if (fHeld <= 0.0)
                        fGate = false;
                }
            }
            else if (d1 == 123.0 || d1 == 120.0)
                allOff();
        }
    }

    double sample()
    {
        while (fMqI < fMqN && double(fMq[fMqI].offset) <= fSi)
        {
            midiEv(fMq[fMqI].st, fMq[fMqI].d1, fMq[fMqI].d2);
            ++fMqI;
        }
        fSi += 1.0;

        if (fSrc > 0)
        {
            const double ct = fSrc == 1 ? fCc1 : fSrc == 2 ? fCc2 : fSrc == 3 ? fAt : fCc11;
            fLev += (ct - fLev) * fKsm;
        }
        if (fGate)
            fEnv += fKa * (1.0 - fEnv);
        else
        {
            fEnv -= fKr * fEnv;
            if (fEnv < 0.00001)
                fEnv = 0.0;
        }
        fBr = std::min(1.0, fEnv * fLev * fAmt);
        const double br = fBr;
        const double ge = std::min(1.0, fEnv * 20.0) * std::min(1.0, fLev * fAmt * 20.0);

        double y = 0.0;
        if (fNcur >= 0.0)
        {
            fNcur += (fNtgt - fNcur) * fKg;
            const double np = fNcur + 2.0 * fBendv;
            const double f = 440.0 * std::pow(2.0, (np - 69.0) / 12.0);
            fNz1 = fNz;
            fNz = noise() * fNf;
            fVph += fVrate / fSr;
            fVph -= std::floor(fVph);
            const double vb = std::sin(2.0 * M_PI * fVph) * std::min(1.0, br * 1.2) * fVib *
                              std::max(0.0, std::min(1.0, (fTn - fVdel) / fVfade));
            fTn += 1.0;
            const double nz = fNz, nz1 = fNz1;

            if (fModel == 0)
            {
                const double d = std::max(2.0, 2.0 * fSr / f - 0.6);
                const double gt = std::min(1.0, br * 60.0);
                const double p = gt * (1.0 + 0.3 * br);
                const double rp = 0.06 * (nz - nz1) * p + p * 0.08 * vb;
                double t = rd(fA, fWa, d);
                fLp = 0.35 * t + 0.65 * fLp;
                t = fLp - fDcX + fDcR * fDcY;
                fDcX = fLp;
                fDcY = t;
                double pd = p + rp - 0.5 * t;
                fB[size_t(fWb)] = pd;
                if (++fWb >= fDL)
                    fWb = 0;
                pd = rd(fB, fWb, d * 0.25);
                const double j = std::max(-1.0, std::min(1.0, pd * (pd * pd - 1.0)));
                fA[size_t(fWa)] = j + 0.5 * t;
                if (++fWa >= fDL)
                    fWa = 0;
                y = 0.176 * t * (0.35 + 0.65 * br);
            }
            else if (fModel == 1)
            {
                const double d = std::max(1.0, 0.5 * fSr / f - 0.5);
                double p = (0.55 + 0.42 * br) * ge;
                p += p * (0.12 * nz + 0.05 * vb);
                const double o = rd(fA, fWa, d);
                const double fl = 0.5 * (o + fZ1);
                fZ1 = o;
                const double pd = -0.95 * fl - p;
                const double r = std::max(-1.0, std::min(1.0, 0.7 - 0.3 * pd));
                fA[size_t(fWa)] = p + pd * r;
                if (++fWa >= fDL)
                    fWa = 0;
                y = o * 0.366 * (0.25 + 0.75 * br);
            }
            else if (fModel == 2)
            {
                const double d = std::max(2.0, fSr / f - 0.5);
                double p = (0.75 + 0.1 * br) * ge;
                p += p * (0.05 * (nz - nz1) + 0.05 * vb);
                const double a = rd(fA, fWa, 0.8 * d);
                const double fl = 0.5 * (a + fZ1);
                fZ1 = a;
                const double t = -0.95 * fl;
                const double o = t - rd(fB, fWb, 0.2 * d);
                const double pd = p - o;
                const double r = std::max(-1.0, std::min(1.0, 0.7 + 0.3 * pd));
                fB[size_t(fWb)] = t;
                if (++fWb >= fDL)
                    fWb = 0;
                fA[size_t(fWa)] = p - pd * r - t;
                if (++fWa >= fDL)
                    fWa = 0;
                y = o * 0.3 * (0.25 + 0.75 * br);
            }
            else
            {
                const double bi = std::max(0.0, std::min(11.999, (np - 40.0) / 4.0));
                const double bk = std::floor(bi);
                const int k = int(bk);
                const double d = (2.15 * fSr / f + fBofs) * std::pow(2.0, (kBT[k] + (kBT[k + 1] - kBT[k]) * (bi - bk)) / 1200.0);
                const double w = 2.0 * M_PI * std::min(f, 0.45 * fSr) / fSr;
                const double lc = 2.0 * fLipR * std::cos(w);
                const double lg = 3.0 * (1.0 - lc + fLipR * fLipR);
                const double gt = ge;
                const double bb = gt * (0.5 + 0.5 * br);
                const double mp = 0.3 * (bb + 0.02 * vb + 0.01 * nz * bb);
                const double bp = 0.85 * rd(fA, fWa, d);
                const double dp = mp - bp;
                const double lpy = lg * dp + lc * fLy1 - fLipR * fLipR * fLy2;
                fLy2 = fLy1;
                fLy1 = lpy;
                const double q = std::min(1.0, lpy * lpy);
                const double o = q * mp + (1.0 - q) * bp;
                const double t = o - fDcX + fDcR * fDcY;
                fDcX = o;
                fDcY = t;
                fA[size_t(fWa)] = t;
                if (++fWa >= fDL)
                    fWa = 0;
                y = bp * 2.23 * (0.4 + 0.6 * br);
            }
        }

        fToneY = y * (1.0 - fToneA) + fToneA * fToneY;
        return fToneY * fVol;
    }

    double fSr = 48000.0;
    int fDL = 2896;
    std::vector<double> fA, fB;
    double fKH[128] = {};
    double fSeed = 12345.0, fHeld = 0.0, fKorder = 0.0;
    bool fSus = false, fGate = false;
    double fNcur = -1.0, fNtgt = 60.0, fLev = 0.0, fEnv = 0.0, fBr = 0.0;
    double fBendv = 0.0, fCc1 = 0.0, fCc2 = 0.0, fCc11 = 0.0, fAt = 0.0;
    double fTn = 0.0, fVph = 0.0, fNz = 0.0, fNz1 = 0.0;
    int fWa = 0, fWb = 0, fModel = 0, fModelWas = -1, fSrc = 0, fNlo = 60, fNhi = 96;
    double fToneY = 0.0, fLp = 0.0, fDcX = 0.0, fDcY = 0.0, fZ1 = 0.0, fLy1 = 0.0, fLy2 = 0.0;
    double fAmt = 0.9, fNf = 1.0, fToneA = 0.0, fVib = 0.75, fVrate = 5.3, fVdel = 0.0, fVfade = 1.0;
    double fKa = 0.0, fKr = 0.0, fKg = 1.0, fKsm = 0.0, fVol = 1.0, fLipR = 0.997, fBofs = 3.0, fDcR = 0.995;

    MidiEv fMq[kMaxEvents];
    uint32_t fMqN = 0, fMqI = 0;
    double fSi = 0.0;
};

} // namespace nullwind
