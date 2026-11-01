/*
 * NULL JSFX - Keys, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/keys.jsfx, computed in double like EEL2 and in the same
 * order: 16 voices of a two-operator FM tone with up to three modal partials and a mallet
 * noise, then Drive, the tremolo and the chorus. Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullkeys {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 14;
static constexpr int kMaxEvents = 512;
static constexpr int NV = 16;
static constexpr int kNumModels = 9;

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

// the models (MT): modulator ratio, FM index, index decay (s), fast and slow decay (s at C4),
// share of the slow decay, decay factor per octave, three partials (ratio, level, decay in s),
// noise level, noise decay (s), attack (ms), noise brightness (Hz), level
static const double kModels[kNumModels][21] = {
    { 1, 1.6, 0.25, 0.7, 4.5, 0.55, 0.72, 7.1, 0.1, 0.3, 13.4, 0.04, 0.1, 0, 0, 0, 0.04, 0.004, 1, 3000, 1.1 },          // Tine EP
    { 1, 2.6, 0.45, 0.5, 2.8, 0.5, 0.7, 3, 0.06, 0.4, 5, 0.03, 0.25, 0, 0, 0, 0.03, 0.003, 1.5, 2500, 1 },               // Reed EP
    { 1, 1.1, 0.8, 1, 3.5, 0.5, 0.75, 14, 0.22, 0.06, 4, 0.04, 0.5, 0, 0, 0, 0, 0.003, 0.5, 4000, 1 },                   // FM EP
    { 1, 0.3, 0.05, 0.9, 2.5, 0.7, 0.8, 6.27, 0.3, 0.35, 17.55, 0.12, 0.08, 0, 0, 0, 0.06, 0.002, 0.3, 6000, 0.75 },     // Music Box
    { 1, 0.5, 0.08, 0.6, 2.2, 0.6, 0.8, 2, 0.1, 0.5, 4, 0.12, 0.25, 8.9, 0.05, 0.06, 0.05, 0.003, 0.5, 5000, 0.85 },     // Celesta
    { 1, 0.2, 0.03, 2.5, 5, 0.8, 0.85, 2.76, 0.35, 1.2, 5.4, 0.25, 0.5, 8.93, 0.15, 0.2, 0.08, 0.002, 0.2, 8000, 0.7 },  // Glockenspiel
    { 1, 0.3, 0.05, 1.5, 7, 0.8, 0.8, 4, 0.25, 0.8, 10, 0.12, 0.25, 0, 0, 0, 0.07, 0.003, 0.5, 4000, 0.75 },             // Vibraphone
    { 1, 0.4, 0.03, 0.25, 1.2, 0.5, 0.65, 4, 0.3, 0.08, 9.9, 0.12, 0.03, 0, 0, 0, 0.12, 0.003, 0.5, 2500, 0.8 },         // Marimba
    { 1, 0.6, 0.04, 0.4, 2, 0.6, 0.75, 5.9, 0.25, 0.12, 13.6, 0.08, 0.04, 0, 0, 0, 0.1, 0.002, 0.3, 3500, 0.85 },        // Kalimba
};

class Keys {
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
            vNote[v] = -1; vGate[v] = 0; vEnv[v] = 0; vPend[v] = -1;
            vPvel[v] = vAge[v] = vPc[v] = vDc[v] = vPm[v] = vDm[v] = vIx[v] = vIxc[v] = 0;
            vA1[v] = vA1c[v] = vA2[v] = vA2c[v] = vNz[v] = vGl[v] = vGr[v] = vRel[v] = 0;
        }
        std::fill(pS1, pS1 + NV * 3, 0.0);
        std::fill(pS2, pS2 + NV * 3, 0.0);
        std::fill(pCf, pCf + NV * 3, 0.0);
        std::fill(pDf, pDf + NV * 3, 0.0);
        std::fill(pE, pE + NV * 3, 0.0);
        std::fill(pR, pR + NV * 3, 0.0);
        std::fill(KD, KD + 128, 0.0);
        sus = false; age = 0;
        std::fill(vCh, vCh + NV, 0.0);
        std::fill(vPp, vPp + NV, 0.0);
        std::fill(vBw, vBw + NV, 0.0);
        std::fill(pW, pW + NV * 3, 0.0);
        std::fill(CHB, CHB + 16, 0.0);
        std::fill(CHP, CHP + 16, 0.0);
        std::fill(KCH, KCH + 128, 0.0);
        curCh = 0; mpeWas = false;
        seed = 31337; nzf = 0;
        trPh = 0; chPh = 0;
        cbSize = int(std::ceil(fSr * 0.025)) + 4;
        CB.assign(size_t(cbSize), 0.0);
        cbW = 0;
    }

    // @slider, from the 14 slider values in order
    void setParams(const double* v)
    {
        const double* s = v - 1; // s[n] = slider n
        const double sr = fSr;
        model = std::max(0.0, std::min(8.0, std::floor(s[1] + 0.5)));
        mb = kModels[int(model)];
        mMr = mb[0]; mMi = mb[1]; mMid = mb[2]; mTf = mb[3]; mTs = mb[4]; mKs = mb[5]; mKt = mb[6];
        mNz = mb[16]; mNd = mb[17]; mAtt = mb[18]; mLp = mb[19]; mGain = mb[20];
        toneK = std::pow(2.0, (s[2] - 50) / 25);
        toneP = 0.4 + 0.012 * s[2];
        bellK = s[3] / 50;
        dsc = s[4] / 100;
        rC = std::exp(-6.907755 / (s[5] * 0.001 * sr));
        vs = s[6] / 100;
        trDt = s[7] / sr;
        tdep = s[8] / 100;
        trSt = s[9] >= 0.5;
        drv = s[10] / 100;
        drive = 1 + 5 * drv;
        drvN = 1 / std::sqrt(drive);
        chm = s[11] / 100;
        chWet = 0.6 * chm;
        chDry = 1 - 0.3 * chm;
        chDt = 0.7 / sr;
        chC = 0.007 * sr;
        chD = 0.0025 * sr;
        vol = std::pow(10.0, s[12] / 20);
        mpe = s[13] >= 0.5;
        mpeR = s[14];
        mpeSm = 1 - std::exp(-1000 / (5 * sr));
        // MPE turned on starts from neutral channels
        if (mpe && !mpeWas)
        {
            std::fill(CHB, CHB + 16, 0.0);
            std::fill(CHP, CHP + 16, 0.0);
        }
        mpeWas = mpe;
        aInc = 1 / (mAtt * 0.001 * sr);
        sInc = 1 / (0.002 * sr);
        nzc = std::exp(-6.907755 / (mNd * sr));
        nzk = std::exp(-2 * kPi * std::min(0.45 * sr, mLp * toneK) / sr);
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
    double trPh = 0;

private:
    void vStart(int v, double n, double vel)
    {
        const double sr = fSr;
        vNote[v] = n; vPend[v] = -1;
        vCh[v] = mpe ? KCH[int(n)] : 0;
        vPp[v] = CHP[int(vCh[v])];
        vBw[v] = 1;
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
        vPc[v] = 0; vPm[v] = 0;
        vDc[v] = f / sr;
        double t = f * mMr;
        vDm[v] = t / sr;
        double ix = mMi * toneK;
        ix *= std::min(1.5, std::sqrt(261.63 / f));
        t = vs * (vn - 0.6);
        ix *= std::max(0.2, 1 + 1.5 * t);
        vIx[v] = ix;
        vIxc[v] = std::exp(-6.907755 / (mMid * sr));
        t = mTf * dsc;
        t *= kdec;
        vA1c[v] = std::exp(-6.907755 / (t * sr));
        t = mTs * dsc;
        t *= kdec;
        vA2c[v] = std::exp(-6.907755 / (t * sr));
        vA1[v] = g * (1 - mKs);
        vA2[v] = g * mKs;
        int b = v * 3;
        const double* p = mb + 7;
        for (int k = 0; k < 3; ++k, ++b, p += 3)
        {
            const double r = p[0];
            double a = p[1] * bellK;
            a *= toneP;
            a *= g;
            t = f * r;
            if (r > 0 && a > 0 && t < 0.45 * sr)
            {
                double w = tpi * t;
                w /= sr;
                t = p[2] * dsc;
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
                pW[b] = w;
            }
            else
            {
                pE[b] = 0; pS1[b] = 0; pS2[b] = 0;
            }
        }
        t = 0.5 + vn;
        vNz[v] = mNz * g;
        vNz[v] *= t;
        const double pan = std::max(-1.0, std::min(1.0, (n - 60) / 30));
        vGl[v] = 1 - 0.3 * pan;
        vGr[v] = 1 + 0.3 * pan;
    }

    void noteOn(double n, double vel)
    {
        KD[int(n)] = 1;
        KCH[int(n)] = curCh;
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
            if (vPend[v] < 0 && vNote[v] == n && (!mpe || vCh[v] == curCh))
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
                if (vPend[v] < 0 && vNote[v] == n && (!mpe || vCh[v] == curCh))
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
            vGate[v] = 0;
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        curCh = double(int(st) & 0x0F);
        if (mpe && curCh > 0 && (t == 0xE0 || t == 0xD0))
        {
            // MPE: a member channel's bend and pressure belong to its note
            if (t == 0xE0)
                CHB[int(curCh)] = (d2 * 128 + d1 - 8192) / 8192;
            else
                CHP[int(curCh)] = d1 / 127;
        }
        else if (t == 0x90 && d2 > 0)
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

        seed *= 16807;
        seed -= std::floor(seed / 2147483647) * 2147483647;
        const double nz = seed / 1073741824 - 1;
        nzf = (1 - nzk) * nz + nzk * nzf;

        double sl = 0, sr = 0;
        for (int v = 0; v < NV; ++v)
        {
            if (vNote[v] < 0)
                continue;
            // MPE: the note's own bend (the partials retuned when it moves) and pressure (5 ms)
            double pc, br = 1;
            if (mpe)
            {
                const int ch = int(vCh[v]);
                br = std::pow(2.0, mpeR * CHB[ch] / 12);
                if (br != vBw[v])
                {
                    vBw[v] = br;
                    for (int b = v * 3; b < v * 3 + 3; ++b)
                        pCf[b] = 2 * pR[b] * std::cos(std::min(2.8, pW[b] * br));
                }
                vPp[v] += (CHP[ch] - vPp[v]) * mpeSm;
                pc = vPc[v] + vDc[v] * br;
            }
            else
                pc = vPc[v] + vDc[v];
            pc -= std::floor(pc);
            vPc[v] = pc;
            double pm = mpe ? vPm[v] + vDm[v] * br : vPm[v] + vDm[v];
            pm -= std::floor(pm);
            vPm[v] = pm;
            const double mo = vIx[v] > 0 ? vIx[v] * std::sin(tpi * pm) : 0;
            const double amp = vA1[v] + vA2[v];
            double y = amp * std::sin(tpi * pc + mo);
            vIx[v] *= vIxc[v];
            if (vIx[v] < 0.00001)
                vIx[v] = 0;
            vA1[v] *= vA1c[v];
            if (vA1[v] < 0.0000001)
                vA1[v] = 0;
            vA2[v] *= vA2c[v];
            double life = amp;
            int b = v * 3;
            for (int k = 0; k < 3; ++k, ++b)
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
            y += vNz[v] * nzf;
            life += vNz[v];
            vNz[v] *= nzc;
            if (vNz[v] < 0.0000001)
                vNz[v] = 0;
            double e = vEnv[v];
            if (vPend[v] >= 0)
                e = std::max(0.0, e - sInc);
            else
                e = std::min(1.0, e + aInc);
            vEnv[v] = e;
            double q = vRel[v];
            if (vGate[v] <= 0)
            {
                q *= rC;
                vRel[v] = q;
            }
            y *= e;
            y *= q;
            if (mpe)
                y *= 1 + vPp[v];
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

        if (drv > 0)
        {
            double u = std::max(-3.0, std::min(3.0, l * drive));
            double t = u * u;
            l = u * (27 + t);
            l /= 27 + 9 * t;
            l *= drvN;
            u = std::max(-3.0, std::min(3.0, r * drive));
            t = u * u;
            r = u * (27 + t);
            r /= 27 + 9 * t;
            r *= drvN;
        }

        trPh += trDt;
        trPh -= std::floor(trPh);
        if (tdep > 0)
        {
            const double lf = 0.5 * std::sin(tpi * trPh);
            if (trSt)
            {
                l *= 1 - tdep * (0.5 + lf);
                r *= 1 - tdep * (0.5 - lf);
            }
            else
            {
                const double t = 1 - tdep * (0.5 + lf);
                l *= t;
                r *= t;
            }
        }

        CB[size_t(cbW)] = 0.5 * (l + r);
        chPh += chDt;
        chPh -= std::floor(chPh);
        if (chm > 0)
        {
            const double lf = chD * std::sin(tpi * chPh);
            double rp = cbW - (chC + lf);
            if (rp < 0)
                rp += cbSize;
            int i = int(std::floor(rp));
            double f = rp - i;
            int i2 = i + 1;
            if (i2 >= cbSize)
                i2 = 0;
            const double cl = CB[size_t(i)] + (CB[size_t(i2)] - CB[size_t(i)]) * f;
            rp = cbW - (chC - lf);
            if (rp < 0)
                rp += cbSize;
            i = int(std::floor(rp));
            f = rp - i;
            i2 = i + 1;
            if (i2 >= cbSize)
                i2 = 0;
            const double cr = CB[size_t(i)] + (CB[size_t(i2)] - CB[size_t(i)]) * f;
            l = l * chDry + cl * chWet;
            r = r * chDry + cr * chWet;
        }
        cbW += 1;
        if (cbW >= cbSize)
            cbW = 0;

        outL = l * vol;
        outR = r * vol;
    }

    static constexpr double tpi = 2 * kPi;

    double fSr = 44100;
    double vNote[NV] = {}, vGate[NV] = {}, vEnv[NV] = {}, vPend[NV] = {}, vPvel[NV] = {}, vAge[NV] = {};
    double vPc[NV] = {}, vDc[NV] = {}, vPm[NV] = {}, vDm[NV] = {}, vIx[NV] = {}, vIxc[NV] = {};
    double vA1[NV] = {}, vA1c[NV] = {}, vA2[NV] = {}, vA2c[NV] = {}, vNz[NV] = {}, vGl[NV] = {}, vGr[NV] = {}, vRel[NV] = {};
    double pS1[NV * 3] = {}, pS2[NV * 3] = {}, pCf[NV * 3] = {}, pDf[NV * 3] = {}, pE[NV * 3] = {}, pR[NV * 3] = {};
    double KD[128] = {};
    // MPE: v_ch, v_pp, v_bw, p_w, CHB, CHP, KCH, cur_ch
    double vCh[NV] = {}, vPp[NV] = {}, vBw[NV] = {}, pW[NV * 3] = {}, CHB[16] = {}, CHP[16] = {}, KCH[128] = {}, curCh = 0;
    bool mpe = false, mpeWas = false;
    double mpeR = 48, mpeSm = 0;
    bool sus = false, trSt = true;
    double age = 0, seed = 31337, nzf = 0, chPh = 0;
    double model = 0;
    const double* mb = kModels[0];
    double mMr = 1, mMi = 0, mMid = 1, mTf = 1, mTs = 1, mKs = 0, mKt = 1, mNz = 0, mNd = 1, mAtt = 1, mLp = 1000, mGain = 1;
    double toneK = 1, toneP = 1, bellK = 1, dsc = 1, rC = 0, vs = 0, trDt = 0, tdep = 0, drv = 0, drive = 1, drvN = 1;
    double chm = 0, chWet = 0, chDry = 1, chDt = 0, chC = 0, chD = 0, vol = 1, aInc = 0, sInc = 0, nzc = 0, nzk = 0;
    std::vector<double> CB;
    int cbSize = 0, cbW = 0;

    MidiEv mq[kMaxEvents];
    uint32_t mqN = 0, mqI = 0;
    double si = 0;
};

} // namespace nullkeys
