/*
 * NULL JSFX - Mallets, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/mallets.jsfx, computed in double like EEL2 and in the same
 * order: 24 voices of six damped resonators at the model's mode ratios, struck by a mallet of
 * a given contact time, then the motor tremolo. Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullmallets {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 11;
static constexpr int kMaxEvents = 512;
static constexpr int NV = 24;
static constexpr int NM = 6;
static constexpr int kNumModels = 6;

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

// the models (MT): six mode ratios, six levels, six decays (s at C4), the level, the decay
// factor per octave, 1 when Resonator acts on the lowest mode
static const double kModels[kNumModels][21] = {
    { 1, 3.99, 9.9, 17.6, 0, 0, 1, 0.3, 0.12, 0.04, 0, 0, 1.4, 0.5, 0.18, 0.08, 0, 0, 0.9, 0.6, 1 },                  // Marimba
    { 1, 3, 6.1, 9.7, 14.3, 0, 1, 0.55, 0.3, 0.15, 0.06, 0, 0.7, 0.35, 0.15, 0.08, 0.05, 0, 0.8, 0.65, 1 },           // Xylophone
    { 1, 4, 10, 17.3, 0, 0, 1, 0.25, 0.1, 0.03, 0, 0, 6, 2.5, 0.8, 0.3, 0, 0, 0.8, 0.75, 1 },                         // Vibraphone
    { 1, 2.76, 5.4, 8.93, 13.34, 0, 1, 0.35, 0.25, 0.12, 0.06, 0, 4.5, 2.5, 1.2, 0.6, 0.3, 0, 0.6, 0.8, 0 },          // Glockenspiel
    { 0.618, 1.209, 2, 2.988, 4.175, 5.55, 0.25, 0.5, 1, 0.8, 0.55, 0.3, 5, 7, 6, 4.5, 3, 2, 0.4, 0.85, 0 },          // Tubular Bells
    { 1, 1.5, 1.98, 2.44, 2.94, 0.63, 1, 0.5, 0.35, 0.2, 0.12, 0.6, 2.2, 1.5, 1.2, 0.9, 0.7, 0.15, 0.55, 0.7, 1 },    // Timpani
};

// what the display needs, from the slider values alone (as the GFX's ml_lvl and ml_dec)
struct View {
    const double* mb = kModels[0];
    double hard = 0.5, strK = 0.55, resA = 1, resT = 1, dsc = 1, vs = 0.6;
    void set(const double* v)
    {
        const double* s = v - 1;
        const double model = std::max(0.0, std::min(5.0, std::floor(s[1] + 0.5)));
        mb = kModels[int(model)];
        hard = s[2] / 100;
        strK = 0.1 + 0.009 * s[3];
        resA = std::pow(2.0, (s[4] - 50) / 50);
        resT = std::pow(2.0, (50 - s[4]) / 100);
        dsc = s[5] / 100;
        vs = s[9] / 100;
    }
    double level(int k) const
    {
        const double h = std::max(0.0, std::min(1.0, hard + vs * 0.3 * (100.0 / 127 - 0.6)));
        const double tc = 0.003 * std::pow(0.08, h);
        double x = 261.6256 * tc;
        x *= x;
        const double w0 = 1 + x * x;
        double a = mb[k + 6];
        if (k == 0 && mb[20] != 0)
            a *= resA;
        if (k == 1 || k == 3 || k == 5)
            a *= strK;
        x = 261.6256 * mb[k] * tc;
        x *= x;
        return a * std::min(1.0, w0 / (1 + x * x));
    }
    double decay(int k) const { return mb[k + 12] * dsc * (k == 0 && mb[20] != 0 ? resT : 1); }
};

class Mallets {
public:
    void setSampleRate(double sr)
    {
        fSr = sr;
        reset();
    }

    // what @init sets
    void reset()
    {
        for (int v = 0; v < NV; ++v)
        {
            vNote[v] = -1; vGate[v] = 0; vEnv[v] = 0; vPend[v] = -1; vRel[v] = 1;
            vPvel[v] = vAge[v] = vGl[v] = vGr[v] = vAi[v] = 0;
        }
        std::fill(pS1, pS1 + NV * NM, 0.0);
        std::fill(pS2, pS2 + NV * NM, 0.0);
        std::fill(pCf, pCf + NV * NM, 0.0);
        std::fill(pDf, pDf + NV * NM, 0.0);
        std::fill(pE, pE + NV * NM, 0.0);
        std::fill(pR, pR + NV * NM, 0.0);
        std::fill(KD, KD + 128, 0.0);
        sus = false; age = 0;
        mtPh = 0;
    }

    // @slider, from the 11 slider values in order
    void setParams(const double* v)
    {
        const double* s = v - 1; // s[n] = slider n
        const double sr = fSr;
        model = std::max(0.0, std::min(5.0, std::floor(s[1] + 0.5)));
        mb = kModels[int(model)];
        mGain = mb[18]; mKt = mb[19]; mRes = mb[20];
        hard = s[2] / 100;
        strK = 0.1 + 0.009 * s[3];
        resA = std::pow(2.0, (s[4] - 50) / 50);
        resT = std::pow(2.0, (50 - s[4]) / 100);
        dsc = s[5] / 100;
        damp = s[6] >= 0.5;
        rC = std::exp(-6.907755 / (s[7] * 0.001 * sr));
        mtDt = s[8] / sr;
        vs = s[9] / 100;
        wd = s[10] / 100;
        vol = std::pow(10.0, s[11] / 20);
        sInc = 1 / (0.002 * sr);
    }

    void beginBlock(const MidiEv* ev, uint32_t count)
    {
        mqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + mqN, mq);
        mqI = 0;
        si = 0;
    }

    void process(float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            double l, r;
            sample(l, r);
            outL[n] = float(l);
            outR[n] = float(r);
        }
    }

    // for the display: the notes sounding (a bit per note in 8 words of 16 bits)
    void soundingBits(uint32_t* bits) const
    {
        for (int i = 0; i < 8; ++i)
            bits[i] = 0;
        for (int v = 0; v < NV; ++v)
        {
            const int n = int(vNote[v]);
            if (n >= 0 && n < 128 && vPend[v] < 0)
                bits[n / 16] |= 1u << (n % 16);
        }
    }
    int voices() const
    {
        int n = 0;
        for (int v = 0; v < NV; ++v)
            n += vNote[v] >= 0;
        return n;
    }

private:
    void vStart(int v, double n, double vel)
    {
        const double sr = fSr;
        vNote[v] = n; vPend[v] = -1;
        vGate[v] = KD[int(n)] > 0 || sus ? 1 : 0;
        vEnv[v] = 0;
        vRel[v] = 1;
        age += 1;
        vAge[v] = age;
        const double f = 440 * std::pow(2.0, (n - 69) / 12);
        const double oc = (n - 60) / 12;
        const double kdec = std::pow(mKt, oc);
        const double vn = vel / 127;
        double g = vn * vn;
        g = 1 - vs + vs * g;
        g *= mGain;
        double h = hard + vs * 0.3 * (vn - 0.6);
        h = std::max(0.0, std::min(1.0, h));
        const double tc = 0.003 * std::pow(0.08, h);
        vAi[v] = 1 / (tc * sr);
        double x = f * tc;
        x *= x;
        const double w0 = 1 + x * x;
        int b = v * NM;
        for (int k = 0; k < NM; ++k, ++b)
        {
            const double r = mb[k];
            double a = mb[k + 6] * g;
            double t = mb[k + 12];
            if (k == 0 && mRes != 0)
            {
                a *= resA;
                t *= resT;
            }
            if (k == 1 || k == 3 || k == 5)
                a *= strK;
            x = f * r;
            x *= tc;
            x *= x;
            a *= std::min(1.0, w0 / (1 + x * x));
            x = f * r;
            if (r > 0 && a > 0 && x < 0.45 * sr)
            {
                double w = tpi * x;
                w /= sr;
                t *= dsc;
                t *= kdec;
                const double rr = std::exp(-6.907755 / (t * sr));
                t = 2 * rr;
                pCf[b] = t * std::cos(w);
                pDf[b] = rr * rr;
                pS1[b] = 0;
                t = a * std::sin(w);
                pS2[b] = -t / rr;
                pE[b] = a;
                pR[b] = rr;
            }
            else
            {
                pE[b] = 0; pS1[b] = 0; pS2[b] = 0;
            }
        }
        double t = std::max(-1.0, std::min(1.0, (n - 60) / 30));
        t *= wd;
        vGl[v] = 1 - 0.6 * t;
        vGr[v] = 1 + 0.6 * t;
    }

    void noteOn(double n, double vel)
    {
        KD[int(n)] = 1;
        int f = -1;
        for (int v = 0; v < NV; ++v)
            if (vPend[v] == n)
            {
                f = v;
                vPvel[v] = vel;
            }
        if (f >= 0)
            return;
        for (int v = 0; v < NV; ++v)
            if (vPend[v] < 0 && vNote[v] == n)
                vGate[v] = 0;
        for (int v = 0; v < NV; ++v)
            if (f < 0 && vNote[v] < 0)
                f = v;
        double ba = 0;
        if (f < 0)
        {
            for (int v = 0; v < NV; ++v)
                if (vGate[v] == 0 && vPend[v] < 0 && (f < 0 || vAge[v] < ba))
                {
                    f = v;
                    ba = vAge[v];
                }
        }
        if (f < 0)
        {
            for (int v = 0; v < NV; ++v)
                if (f < 0 || vAge[v] < ba)
                {
                    f = v;
                    ba = vAge[v];
                }
        }
        if (vNote[f] < 0)
            vStart(f, n, vel);
        else
        {
            vGate[f] = 0;
            vPend[f] = n;
            vPvel[f] = vel;
            age += 1;
            vAge[f] = age;
        }
    }

    void noteOff(double n)
    {
        KD[int(n)] = 0;
        if (!sus)
            for (int v = 0; v < NV; ++v)
                if (vPend[v] < 0 && vNote[v] == n)
                    vGate[v] = 0;
    }

    void susOff()
    {
        sus = false;
        for (int v = 0; v < NV; ++v)
            if (vPend[v] < 0 && vNote[v] >= 0 && KD[int(vNote[v])] == 0)
                vGate[v] = 0;
    }

    void allOff()
    {
        std::fill(KD, KD + 128, 0.0);
        sus = false;
        for (int v = 0; v < NV; ++v)
        {
            vGate[v] = 0;
            vRel[v] = std::min(vRel[v], 0.999);
        }
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        if (t == 0x90 && d2 > 0)
            noteOn(d1, d2);
        else if (t == 0x80 || t == 0x90)
            noteOff(d1);
        else if (t == 0xB0)
        {
            if (d1 == 64)
            {
                if (d2 >= 64)
                    sus = true;
                else if (sus)
                    susOff();
            }
            else if (d1 == 123 || d1 == 120)
                allOff();
        }
    }

    void sample(double& outL, double& outR)
    {
        while (mqI < mqN && double(mq[mqI].offset) <= si)
        {
            midiEv(mq[mqI].st, mq[mqI].d1, mq[mqI].d2);
            ++mqI;
        }
        si += 1;

        double sl = 0, sr = 0;
        for (int v = 0; v < NV; ++v)
        {
            if (vNote[v] < 0)
                continue;
            double y = 0;
            double life = 0;
            int b = v * NM;
            for (int k = 0; k < NM; ++k, ++b)
            {
                if (pE[b] > 0)
                {
                    const double r = pCf[b] * pS1[b] - pDf[b] * pS2[b];
                    pS2[b] = pS1[b];
                    pS1[b] = r;
                    y += r;
                    life += pE[b];
                    pE[b] *= pR[b];
                    if (pE[b] < 0.000001)
                        pE[b] = 0;
                }
            }
            double e = vEnv[v];
            if (vPend[v] >= 0)
                e = std::max(0.0, e - sInc);
            else
                e = std::min(1.0, e + vAi[v]);
            vEnv[v] = e;
            double q = vRel[v];
            if ((damp && vGate[v] <= 0) || q < 1)
            {
                q *= rC;
                vRel[v] = q;
            }
            y *= e;
            y *= q;
            sl += y * vGl[v];
            sr += y * vGr[v];
            if (vPend[v] >= 0)
            {
                if (e <= 0)
                    vStart(v, vPend[v], vPvel[v]);
            }
            else if (q < 0.0001 || life < 0.00002)
                vNote[v] = -1;
        }
        double l = sl * 0.25;
        double r = sr * 0.25;

        mtPh += mtDt;
        mtPh -= std::floor(mtPh);
        if (mtDt > 0)
        {
            const double t = 1 - 0.35 * (1 - std::cos(tpi * mtPh));
            l *= t;
            r *= t;
        }

        outL = l * vol;
        outR = r * vol;
    }

    static constexpr double tpi = 2 * kPi;

    double fSr = 44100;
    double vNote[NV] = {}, vGate[NV] = {}, vEnv[NV] = {}, vPend[NV] = {}, vPvel[NV] = {}, vAge[NV] = {};
    double vGl[NV] = {}, vGr[NV] = {}, vRel[NV] = {}, vAi[NV] = {};
    double pS1[NV * NM] = {}, pS2[NV * NM] = {}, pCf[NV * NM] = {}, pDf[NV * NM] = {}, pE[NV * NM] = {}, pR[NV * NM] = {};
    double KD[128] = {};
    bool sus = false, damp = false;
    double age = 0, mtPh = 0;
    double model = 0;
    const double* mb = kModels[0];
    double mGain = 1, mKt = 1, mRes = 0;
    double hard = 0.5, strK = 0.55, resA = 1, resT = 1, dsc = 1, rC = 0, mtDt = 0, vs = 0, wd = 0, vol = 1, sInc = 0;

    MidiEv mq[kMaxEvents];
    uint32_t mqN = 0, mqI = 0;
    double si = 0;
};

} // namespace nullmallets
