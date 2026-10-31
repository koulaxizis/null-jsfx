/*
 * NULL JSFX - Pluck, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/pluck.jsfx, computed in double like EEL2 and in the same
 * order: 12 Karplus-Strong strings, the strummer on the host's beat, the body and a DC
 * blocker. Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI and the transport, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullpluck {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 13;
static constexpr int kMaxEvents = 512;
static constexpr int NV = 12;
static constexpr int kNumModels = 10;

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

// the models (MT): decay (s at C3), decay factor per octave, loop low-pass, pluck brightness
// (Hz), the start sharp (cents) and how fast it settles (s), three body resonances (Hz, Q,
// level), the triangle's share of the pluck, level
static const double kModels[kNumModels][17] = {
    { 3.5, 0.75, 0.35, 2500, 0, 0.05, 100, 2, 1, 200, 3, 0.6, 400, 3, 0.3, 0.6, 1.25 },        // Nylon Guitar
    { 5, 0.75, 0.18, 5000, 3, 0.03, 110, 2, 0.8, 230, 3, 0.5, 2600, 2, 0.3, 0.4, 0.8 },        // Steel Guitar
    { 7, 0.7, 0.3, 1800, 0, 0.05, 90, 1.5, 1, 180, 2, 0.5, 360, 2, 0.3, 0.8, 0.85 },           // Harp
    { 3, 0.75, 0.15, 6000, 25, 0.08, 160, 3, 0.8, 450, 3, 0.6, 1400, 3, 0.3, 0.3, 0.75 },      // Koto
    { 2, 0.7, 0.12, 7000, 18, 0.05, 250, 3, 0.9, 700, 3, 0.6, 2000, 3, 0.3, 0.3, 0.7 },        // Pipa
    { 1.5, 0.8, 0.1, 8000, 6, 0.02, 350, 4, 1.2, 1100, 4, 0.7, 3000, 3, 0.4, 0.2, 0.6 },       // Banjo
    { 0.25, 0.9, 0.5, 1500, 0, 0.05, 110, 2, 1, 220, 3, 0.6, 440, 3, 0.3, 0.7, 1.4 },          // Muted Guitar
    { 6, 0.75, 0.12, 6000, 2, 0.02, 200, 2, 0.6, 600, 3, 0.5, 1800, 3, 0.3, 0.1, 0.6 },        // Dulcimer
    { 1, 0.7, 0.45, 1500, 0, 0.05, 280, 6, 1.2, 470, 5, 0.8, 2800, 2, 0.4, 0.7, 1 },           // Pizzicato Violins
    { 1.4, 0.75, 0.45, 1000, 0, 0.05, 110, 5, 1.2, 220, 4, 0.8, 1300, 2, 0.3, 0.7, 1.1 },      // Pizzicato Cellos
};

// the strum patterns (PT), 8 steps (0 rest, 1 down, 2 up) for Strum 1..5
static const int kPatterns[6][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 1, 1 }, // Down
    { 2, 2, 2, 2, 2, 2, 2, 2 }, // Up
    { 1, 2, 1, 2, 1, 2, 1, 2 }, // Down-Up
    { 1, 0, 1, 2, 0, 2, 1, 2 }, // Folk
    { 0, 1, 0, 1, 0, 1, 0, 1 }, // Offbeat
};

class Pluck {
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
            vRel[v] = vPvel[v] = vAge[v] = vFast[v] = vW[v] = vD0[v] = vTw[v] = vTwk[v] = 0;
            vLp[v] = vB[v] = vG[v] = vEk[v] = vEn[v] = vPp[v] = vAmp[v] = vGl[v] = vGr[v] = vPk[v] = 0;
        }
        std::fill(KD, KD + 128, 0.0);
        std::fill(HL, HL + 128, 0.0);
        nheld = 0; sus = false; age = 0; lastVel = 100;
        seed = 4711;
        lb = int(std::ceil(fSr / 30)) + 8;
        SB.assign(size_t(NV * lb), 0.0);
        EB.assign(size_t(NV * lb), 0.0);
        stS = -1000; stClock = 0; stOn = false; sc = 0;
        for (int k = 0; k < 3; ++k)
            blS[k] = blT[k] = brS[k] = brT[k] = 0;
        dcXl = dcYl = dcXr = dcYr = 0;
    }

    // @slider, from the 12 slider values in order
    void setParams(const double* v)
    {
        const double* s = v - 1; // s[n] = slider n
        const double sr = fSr;
        const double pz = std::max(0.0, std::min(2.0, std::floor(s[13] + 0.5)));
        model = pz > 0 ? 7 + pz : std::max(0.0, std::min(7.0, std::floor(s[1] + 0.5)));
        mb = kModels[int(model)];
        mT60 = mb[0]; mKt = mb[1]; mLpb = mb[2]; mExb = mb[3]; mTw = mb[4]; mTwt = mb[5];
        triM = mb[15]; mGain = mb[16];
        pp = s[2] / 100;
        lpbE = std::min(0.9, mLpb * std::pow(2.0, (50 - s[3]) / 30));
        exbE = mExb * std::pow(2.0, (s[3] - 50) / 25);
        dsc = s[4] / 100;
        rC = std::exp(-6.907755 / (s[5] * 0.001 * sr));
        rfC = std::exp(-6.907755 / (0.04 * sr));
        bm = s[6] / 100;
        wid = 0.006 * s[7];
        vs = s[8] / 100;
        stMode = std::floor(s[9] + 0.5);
        stRate = std::floor(s[10] + 0.5);
        spb = stRate == 0 ? 1 : stRate == 1 ? 2 : stRate == 2 ? 4 : 3;
        double t = s[11] * 0.001;
        spreadS = std::floor(t * sr + 0.5);
        vol = std::pow(10.0, s[12] / 20);
        twK = 1 - std::pow(2.0, -mTw / 1200);
        twc = std::exp(-1 / (mTwt * sr));
        aInc = 1 / (0.0005 * sr);
        sInc = 1 / (0.002 * sr);
        pkc = std::exp(-1 / (0.03 * sr));
        t = tpi * 20;
        dcR = 1 - t / sr;
        t = 1 + 0.0006 * s[7];
        for (int k = 0; k < 3; ++k)
        {
            bpSet(mb[6 + 3 * k], mb[7 + 3 * k]);
            blK[k] = bpK * mb[8 + 3 * k]; blA1[k] = bpA1; blA2[k] = bpA2; blA3[k] = bpA3;
        }
        for (int k = 0; k < 3; ++k)
        {
            bpSet(mb[6 + 3 * k] * t, mb[7 + 3 * k]);
            brK[k] = bpK * mb[8 + 3 * k]; brA1[k] = bpA1; brA2[k] = bpA2; brA3[k] = bpA3;
        }
        bDry = 1 - 0.5 * bm;
        bWet = 1.5 * bm;
    }

    void beginBlock(const MidiEv* ev, uint32_t count, bool isPlaying, double tempo, double beatPos)
    {
        mqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + mqN, mq);
        mqI = 0;
        playing = isPlaying;
        bInc = tempo / (60 * fSr);
        b0 = beatPos;
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

    // for the display: the notes sounding and the keys held (a bit per note in 8 words of 16
    // bits each), the strum step playing (0..7, -1 none)
    void soundingBits(uint32_t* bits) const
    {
        for (int i = 0; i < 8; ++i)
            bits[i] = 0;
        for (int v = 0; v < NV; ++v)
        {
            const int n = int(vNote[v]);
            if (n >= 0 && n < 128 && vPend[v] < 0 && vGate[v] > 0)
                bits[n / 16] |= 1u << (n % 16);
        }
    }
    void heldBits(uint32_t* bits) const
    {
        for (int i = 0; i < 8; ++i)
            bits[i] = 0;
        for (int n = 0; n < 128; ++n)
            if (HL[n] > 0)
                bits[n / 16] |= 1u << (n % 16);
    }
    int voices() const
    {
        int n = 0;
        for (int v = 0; v < NV; ++v)
            n += vNote[v] >= 0;
        return n;
    }
    int strumStep() const
    {
        if (stMode <= 0 || nheld <= 0 || stS <= -1000)
            return -1;
        return int(stS - std::floor(stS / 8) * 8);
    }

private:
    void bpSet(double f, double q)
    {
        const double g = std::tan(kPi * std::min(0.45 * fSr, f) / fSr);
        bpK = 1 / q;
        bpA1 = 1 / (1 + g * (g + bpK));
        bpA2 = g * bpA1;
        bpA3 = g * bpA2;
    }

    void vStart(int v, double n, double vel)
    {
        const double sr = fSr;
        vNote[v] = n; vPend[v] = -1;
        vGate[v] = HL[int(n)] > 0 ? 1 : 0;
        vEnv[v] = 0; vRel[v] = 1; vFast[v] = 0;
        age += 1;
        vAge[v] = age;
        double f = 440 * std::pow(2.0, (n - 69) / 12);
        f = std::max(30.0, std::min(sr / 8, f));
        const double d0 = sr / f;
        double w0 = tpi * f;
        w0 /= sr;
        const double b = lpbE;
        double t = b * std::sin(w0);
        double tt = 1 - b * std::cos(w0);
        const double pd = std::atan2(t, tt) / w0;
        const double hlp = (1 - b) / std::sqrt(t * t + tt * tt);
        const double d = d0 - pd;
        const double fr = d - std::floor(d);
        t = 2 * fr;
        t *= 1 - fr;
        t *= std::cos(w0);
        const double hm = std::sqrt((1 - fr) * (1 - fr) + fr * fr + t);
        vD0[v] = d;
        t = mT60 * dsc;
        t *= std::pow(mKt, (n - 48) / 12);
        tt = t * f;
        double g = std::exp(-6.907755 / tt);
        t = hlp * hm;
        g = std::min(0.9999, g / t);
        vG[v] = g;
        vB[v] = b;
        vLp[v] = 0;
        const double vn = vel / 127;
        double gv = vn * vn;
        gv = 1 - vs + vs * gv;
        gv *= mGain;
        vTw[v] = 1;
        vTwk[v] = twK * (0.5 + vn);
        double* sb = SB.data() + size_t(v * lb);
        std::fill(sb, sb + lb, 0.0);
        vW[v] = 0;
        const double fc = exbE * (0.5 + vn);
        t = tpi * std::min(0.45 * sr, fc);
        const double c = std::exp(-t / sr);
        const double nn = std::max(2.0, std::floor(d0 + 0.5));
        double* ebp = EB.data() + size_t(v * lb);
        double z = 0, s = 0;
        const int nk = int(nn);
        for (int k = 0; k < nk; ++k)
        {
            seed *= 16807;
            seed -= std::floor(seed / 2147483647) * 2147483647;
            z = (1 - c) * (seed / 1073741824 - 1) + c * z;
            const double pos = (k + 0.5) / nn;
            const double tri = pos < pp ? pos / pp : (1 - pos) / (1 - pp);
            double e = 2 * z;
            e *= 1 - triM;
            e += triM * tri;
            ebp[k] = e;
            s += e;
        }
        s /= nn;
        for (int k = 0; k < nk; ++k)
            ebp[k] -= s;
        vEk[v] = 0;
        vEn[v] = nn;
        vPp[v] = std::max(1.0, std::floor(pp * nn + 0.5));
        vAmp[v] = gv;
        vPk[v] = 0;
        double p = std::max(-1.0, std::min(1.0, (n - 60) / 24));
        p *= wid;
        vGl[v] = 1 - p;
        vGr[v] = 1 + p;
    }

    void pluck(double n, double vel)
    {
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
            if (vPend[v] < 0 && vNote[v] == n && vFast[v] == 0)
            {
                vGate[v] = 0;
                vFast[v] = 1;
            }
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

    void countHeld()
    {
        nheld = 0;
        for (int k = 0; k < 128; ++k)
            nheld += HL[k] > 0;
    }
    void release(double n)
    {
        for (int v = 0; v < NV; ++v)
            if (vPend[v] < 0 && vNote[v] == n)
                vGate[v] = 0;
    }
    void noteOn(double n, double vel)
    {
        if (nheld == 0)
            stClock = 0;
        KD[int(n)] = 1;
        HL[int(n)] = 1;
        countHeld();
        lastVel = vel;
        if (stMode == 0)
            pluck(n, vel);
    }
    void noteOff(double n)
    {
        KD[int(n)] = 0;
        if (!sus)
        {
            HL[int(n)] = 0;
            release(n);
            countHeld();
        }
    }
    void susOff()
    {
        sus = false;
        for (int k = 0; k < 128; ++k)
            if (KD[k] == 0 && HL[k] > 0)
            {
                HL[k] = 0;
                release(k);
            }
        countHeld();
    }
    void allOff()
    {
        std::fill(KD, KD + 128, 0.0);
        std::fill(HL, HL + 128, 0.0);
        nheld = 0;
        sus = false;
        stOn = false;
        for (int v = 0; v < NV; ++v)
            vGate[v] = 0;
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

    void stStep(double s)
    {
        const int k = int(s - std::floor(s / 8) * 8);
        const int c = kPatterns[int(stMode)][k];
        if (c > 0)
        {
            stDir = c == 1 ? 1 : -1;
            stMax = c == 2 && (stMode == 3 || stMode == 4) ? 4 : 128;
            double a = s - std::floor(s / spb) * spb;
            a = a == 0 ? 1 : 0.85;
            if (c == 2)
                a *= 0.8;
            stVel = lastVel * a;
            stT0 = sc;
            stI = 0;
            stOn = true;
        }
    }
    void stNext()
    {
        int n = -1;
        if (stI < stMax)
        {
            int j = 0;
            int k = stDir > 0 ? 0 : 127;
            for (int i = 0; i < 128; ++i, k += stDir)
                if (n < 0 && HL[k] > 0)
                {
                    if (j == stI)
                        n = k;
                    else
                        j += 1;
                }
        }
        if (n >= 0)
        {
            pluck(n, stVel * (1 - 0.04 * stI));
            stI += 1;
        }
        else
            stOn = false;
    }

    void sample(double& outL, double& outR)
    {
        while (mqI < mqN && double(mq[mqI].offset) <= si)
        {
            midiEv(mq[mqI].st, mq[mqI].d1, mq[mqI].d2);
            ++mqI;
        }

        if (stMode > 0)
        {
            if (nheld > 0)
            {
                const double bb = playing ? b0 + si * bInc : stClock;
                stClock = bb + bInc;
                const double t = bb * spb;
                const double s = std::floor(t + 0.000001);
                if (s != stS)
                {
                    // a key on the half itself waits, however the host rounds the beat
                    if (stS > -1000 || t - s < 0.499999)
                        stStep(s);
                    stS = s;
                }
            }
            else
            {
                stS = -1000;
                stOn = false;
            }
            while (stOn && sc >= stT0 + stI * spreadS)
                stNext();
        }
        si += 1;
        sc += 1;

        double sl = 0, sr = 0;
        for (int v = 0; v < NV; ++v)
        {
            if (vNote[v] < 0)
                continue;
            const double tw = vTw[v];
            double t = vTwk[v] * tw;
            const double d = vD0[v] * (1 - t);
            vTw[v] = tw * twc;
            if (vTw[v] < 0.000001)
                vTw[v] = 0;
            double* sb = SB.data() + size_t(v * lb);
            int w = int(vW[v]);
            double rp = w - d;
            if (rp < 0)
                rp += lb;
            const int i = int(std::floor(rp));
            const double fr = rp - i;
            int i2 = i + 1;
            if (i2 >= lb)
                i2 = 0;
            const double x = sb[i] + (sb[i2] - sb[i]) * fr;
            const double b = vB[v];
            const double lp = (1 - b) * x + b * vLp[v];
            vLp[v] = lp;
            double sv = lp * vG[v];
            const double k = vEk[v];
            if (k < vEn[v])
            {
                const double* ebuf = EB.data() + size_t(v * lb);
                double ex = ebuf[int(k)];
                if (k >= vPp[v])
                    ex -= ebuf[int(k - vPp[v])];
                sv += ex * vAmp[v];
                vEk[v] = k + 1;
            }
            sb[w] = sv;
            w += 1;
            if (w >= lb)
                w = 0;
            vW[v] = w;
            const double pk = std::max(std::fabs(sv), vPk[v] * pkc);
            vPk[v] = pk;
            double e = vEnv[v];
            if (vPend[v] >= 0)
                e = std::max(0.0, e - sInc);
            else
                e = std::min(1.0, e + aInc);
            vEnv[v] = e;
            double q = vRel[v];
            if (vGate[v] <= 0)
            {
                q *= vFast[v] > 0 ? rfC : rC;
                vRel[v] = q;
            }
            double y = sv * e;
            y *= q;
            sl += y * vGl[v];
            sr += y * vGr[v];
            if (vPend[v] >= 0)
            {
                if (e <= 0)
                    vStart(v, vPend[v], vPvel[v]);
            }
            else if (q < 0.0001 || (vEk[v] >= vEn[v] && pk < 0.00002))
                vNote[v] = -1;
        }

        double l = body(sl, blS, blT, blA1, blA2, blA3, blK);
        double r = body(sr, brS, brT, brA1, brA2, brA3, brK);

        double y = l - dcXl + dcR * dcYl;
        dcXl = l; dcYl = flush(y); l = y;
        y = r - dcXr + dcR * dcYr;
        dcXr = r; dcYr = flush(y); r = y;
        l *= 0.35;
        r *= 0.35;
        outL = l * vol;
        outR = r * vol;
    }

    // a filter state that has died away is zero (no denormals)
    static double flush(double x) { return std::fabs(x) < 0.000000000000001 ? 0 : x; }

    double body(double x, double* s1, double* s2, const double* a1, const double* a2, const double* a3, const double* kk)
    {
        double y = 0;
        for (int k = 0; k < 3; ++k)
        {
            const double t = x - s2[k];
            const double u = a1[k] * s1[k] + a2[k] * t;
            const double u2 = s2[k] + a2[k] * s1[k] + a3[k] * t;
            s1[k] = flush(2 * u - s1[k]);
            s2[k] = flush(2 * u2 - s2[k]);
            if (k == 0)
                y = kk[k] * u;
            else
                y += kk[k] * u;
        }
        return x * bDry + y * bWet;
    }

    static constexpr double tpi = 2 * kPi;

    double fSr = 44100;
    double vNote[NV] = {}, vGate[NV] = {}, vEnv[NV] = {}, vRel[NV] = {}, vPend[NV] = {}, vPvel[NV] = {};
    double vAge[NV] = {}, vFast[NV] = {}, vW[NV] = {}, vD0[NV] = {}, vTw[NV] = {}, vTwk[NV] = {};
    double vLp[NV] = {}, vB[NV] = {}, vG[NV] = {}, vEk[NV] = {}, vEn[NV] = {}, vPp[NV] = {}, vAmp[NV] = {};
    double vGl[NV] = {}, vGr[NV] = {}, vPk[NV] = {};
    double KD[128] = {}, HL[128] = {};
    int nheld = 0;
    bool sus = false, playing = false, stOn = false;
    double age = 0, lastVel = 100, seed = 4711;
    int lb = 0;
    std::vector<double> SB, EB;
    double stS = -1000, stClock = 0, sc = 0, stT0 = 0, stVel = 0;
    int stI = 0, stDir = 1, stMax = 128;
    double model = 0;
    const double* mb = kModels[0];
    double mT60 = 1, mKt = 1, mLpb = 0, mExb = 1000, mTw = 0, mTwt = 1, triM = 0, mGain = 1;
    double pp = 0.2, lpbE = 0, exbE = 1000, dsc = 1, rC = 0, rfC = 0, bm = 0, wid = 0, vs = 0;
    double stMode = 0, stRate = 1, spb = 2, spreadS = 0, vol = 1, twK = 0, twc = 0, aInc = 0, sInc = 0, pkc = 0, dcR = 0;
    double bpK = 1, bpA1 = 0, bpA2 = 0, bpA3 = 0;
    double blS[3] = {}, blT[3] = {}, brS[3] = {}, brT[3] = {};
    double blA1[3] = {}, blA2[3] = {}, blA3[3] = {}, blK[3] = {}, brA1[3] = {}, brA2[3] = {}, brA3[3] = {}, brK[3] = {};
    double bDry = 1, bWet = 0;
    double dcXl = 0, dcYl = 0, dcXr = 0, dcYr = 0;
    double bInc = 0, b0 = 0;

    MidiEv mq[kMaxEvents];
    uint32_t mqN = 0, mqI = 0;
    double si = 0;
};

} // namespace nullpluck
