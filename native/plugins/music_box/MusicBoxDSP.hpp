/*
 * NULL JSFX - Music Box, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/music_box.jsfx, computed in double like EEL2 and in the
 * same order: 32 tines of four damped resonators (a clamped bar's modes), plucked by a pin, a
 * wooden box of four band-pass resonances, the pin's click and the governor's whirr (exact
 * Park-Miller noise), and Wind Down, which holds notes back more and more as the spring runs
 * out. Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullmusicbox {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 9;
static constexpr int kMaxEvents = 512;
static constexpr int NV = 32;
static constexpr int NM = 4;
static constexpr int kQueue = 64;

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

// one Park-Miller step (mb_pm)
inline double pm(double x)
{
    x *= 16807;
    return x - std::floor(x / 2147483647) * 2147483647;
}

// an RBJ biquad, transposed direct form II, with the JSFX's denormal guard (bq_bp, bq_hp, bq)
struct Biquad {
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void bp(double f, double q, double sr)
    {
        const double w = 2 * kPi * std::min(f, 0.45 * sr) / sr, al = std::sin(w) / (2 * q);
        b0 = al / (1 + al); b1 = 0; b2 = -b0;
        a1 = -2 * std::cos(w) / (1 + al); a2 = (1 - al) / (1 + al);
    }
    void hp(double f, double q, double sr)
    {
        const double w = 2 * kPi * std::min(f, 0.45 * sr) / sr, al = std::sin(w) / (2 * q);
        b0 = (1 + std::cos(w)) / 2 / (1 + al); b1 = -(1 + std::cos(w)) / (1 + al); b2 = b0;
        a1 = -2 * std::cos(w) / (1 + al); a2 = (1 - al) / (1 + al);
    }
    double tick(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::fabs(z1) < 0.000000000000000000001) z1 = 0;
        if (std::fabs(z2) < 0.000000000000000000001) z2 = 0;
        return y;
    }
};

static const double kRatio[NM] = { 1, 5.93, 16.2, 31.7 };
static const double kDecay[NM] = { 3.2, 0.7, 0.25, 0.09 };
static const double kLevel[NM] = { 1, 0.32, 0.12, 0.05 };

class MusicBox {
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
            vNote[v] = -1; vBuzz[v] = 0; vBph[v] = 0; vDmp[v] = 0; vLf[v] = 0;
            vF[v] = vGl[v] = vGr[v] = 0;
        }
        std::fill(pS1, pS1 + NV * NM, 0.0);
        std::fill(pS2, pS2 + NV * NM, 0.0);
        std::fill(pE, pE + NV * NM, 0.0);
        std::fill(pCf, pCf + NV * NM, 0.0);
        std::fill(pDf, pDf + NV * NM, 0.0);
        std::fill(pR, pR + NV * NM, 0.0);
        wqN = 0; wdD = 0; wdDead = false; wdIdle = 0; cnt = 0;
        stIdle = 1000000000;
        ck = 0; gov = 0; whG = 0;
        seedC = 12345; seedW = 67890;
        for (int k = 0; k < 4; ++k)
        {
            bl[k].z1 = bl[k].z2 = 0;
            br[k].z1 = br[k].z2 = 0;
        }
        hc.z1 = hc.z2 = hw.z1 = hw.z2 = 0;
        hc.hp(3000, 0.7, fSr);
        hw.bp(700, 1.5, fSr);
        ckC = std::exp(-1 / (0.0015 * fSr));
        bzC = std::exp(-1 / (0.012 * fSr));
        dmC = std::exp(-1 / (0.01 * fSr));
        whC = std::exp(-1 / (0.15 * fSr));
    }

    // @slider, from the 9 slider values in order
    void setParams(const double* v)
    {
        const double* s = v - 1; // s[n] = slider n
        const double sr = fSr;
        dsc = s[1] / 100;
        bri = std::pow(2.0, (s[2] - 50) / 50);
        body = s[3] / 100;
        const double bx = std::pow(2.0, (50 - s[4]) / 50);
        static const double kF[4] = { 240, 510, 980, 1850 }, kQ[4] = { 6, 8, 10, 12 };
        for (int k = 0; k < 4; ++k)
            bl[k].bp(kF[k] * bx, kQ[k], sr);
        for (int k = 0; k < 4; ++k)
            br[k].bp(kF[k] * bx * 1.04, kQ[k], sr);
        mech = s[5] / 100;
        wdAmt = s[6] / 100;
        vs = s[7] / 100;
        wd = s[8] / 100;
        vol = std::pow(10.0, s[9] / 20);
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

    // for the display: the tines ringing (a bit per note in 8 words of 16 bits)
    void soundingBits(uint32_t* bits) const
    {
        for (int i = 0; i < 8; ++i)
            bits[i] = 0;
        for (int v = 0; v < NV; ++v)
        {
            const int n = int(vNote[v]);
            if (n >= 0 && n < 128)
                bits[n / 16] |= 1u << (n % 16);
        }
    }
    int tines() const
    {
        int n = 0;
        for (int v = 0; v < NV; ++v)
            n += vNote[v] >= 0;
        return n;
    }
    // the spring: 1 wound, 0 run down (as the GFX's SPRING %), -1 when it has stopped
    double spring() const { return wdDead ? -1.0 : 1.0 - std::min(1.0, wdD / (3 * fSr)); }

private:
    void strike(double n, double vel)
    {
        const double sr = fSr;
        int f = -1;
        for (int v = 0; v < NV; ++v)
            if (f < 0 && vNote[v] == n) f = v;
        for (int v = 0; v < NV; ++v)
            if (f < 0 && vNote[v] < 0) f = v;
        if (f < 0)
        {
            double ba = 0;
            for (int v = 0; v < NV; ++v)
                if (f < 0 || vLf[v] < ba)
                {
                    f = v;
                    ba = vLf[v];
                }
        }
        const int v = f;
        double ring = 0;
        if (vNote[v] == n)
        {
            for (int k = 0, b = v * NM; k < NM; ++k, ++b)
                ring += std::fabs(pS1[b]);
            if (ring > 0.001)
                vBuzz[v] = std::min(1.0, ring * 4) * mech;
        }
        else
        {
            for (int k = 0, b = v * NM; k < NM; ++k, ++b)
                pS1[b] = pS2[b] = pE[b] = 0;
            vBuzz[v] = 0;
            vBph[v] = 0;
        }
        vNote[v] = n;
        vDmp[v] = 0;
        const double fq = 440 * std::pow(2.0, (n - 69) / 12);
        vF[v] = fq;
        const double oc = (n - 72) / 12;
        const double vn = vel / 127;
        const double g = 1 - vs + vs * vn;
        for (int k = 0, b = v * NM; k < NM; ++k, ++b)
        {
            const double fr = fq * kRatio[k];
            if (fr < 0.45 * sr)
            {
                double t = kDecay[k] * dsc * std::pow(0.62, oc);
                t = std::min(t, 30.0);
                const double rr = std::exp(-6.907755 / (t * sr));
                const double w = tpi * fr / sr;
                const double a = kLevel[k] * std::pow(bri, double(k)) * g * (1 + 0.15 * k * vn);
                pCf[b] = 2 * rr * std::cos(w);
                pDf[b] = rr * rr;
                pS1[b] += a * std::sin(w);
                pE[b] += a;
                pR[b] = rr;
            }
            else
            {
                pS1[b] = 0; pS2[b] = 0; pE[b] = 0;
            }
        }
        const double t = std::max(-1.0, std::min(1.0, (n - 76) / 18)) * 0.5 * wd;
        vGl[v] = 1 - t;
        vGr[v] = 1 + t;
        ck = std::max(ck, 0.02 * vn * (0.3 + mech));
        stIdle = 0;
    }

    void key(double n, double vel)
    {
        if (wdAmt > 0)
        {
            if (!wdDead)
            {
                if (wdD < 0.5)
                    strike(n, vel);
                else if (wqN < kQueue)
                {
                    wq[wqN][0] = cnt + wdD;
                    wq[wqN][1] = n;
                    wq[wqN][2] = vel;
                    wqN += 1;
                }
                wdD = wdD * (1 + 0.1 * wdAmt) + 0.01 * wdAmt * fSr;
                if (wdD > 3 * fSr)
                    wdDead = true;
            }
        }
        else
            strike(n, vel);
        wdIdle = 0;
    }

    void allOff()
    {
        for (int v = 0; v < NV; ++v)
            if (vNote[v] >= 0)
                vDmp[v] = 1;
        wqN = 0;
        wdD = 0;
        wdDead = false;
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        if (t == 0x90 && d2 > 0)
            key(d1, d2);
        else if (t == 0xB0 && (d1 == 123 || d1 == 120))
            allOff();
    }

    void sample(double& outL, double& outR)
    {
        const double sr = fSr;
        while (mqI < mqN && double(mq[mqI].offset) <= si)
        {
            midiEv(mq[mqI].st, mq[mqI].d1, mq[mqI].d2);
            ++mqI;
        }
        si += 1;

        if (wqN > 0)
        {
            int q = 0;
            while (q < wqN)
            {
                if (wq[q][0] <= cnt)
                {
                    strike(wq[q][1], wq[q][2]);
                    wqN -= 1;
                    wq[q][0] = wq[wqN][0];
                    wq[q][1] = wq[wqN][1];
                    wq[q][2] = wq[wqN][2];
                }
                else
                    q += 1;
            }
        }
        cnt += 1;
        wdIdle += 1;
        if (wdIdle > 1.5 * sr && wqN == 0)
        {
            wdD = 0;
            wdDead = false;
        }
        stIdle += 1;

        double sl = 0, sR = 0;
        for (int v = 0; v < NV; ++v)
        {
            if (vNote[v] < 0)
                continue;
            double y = 0;
            double life = 0;
            for (int k = 0, b = v * NM; k < NM; ++k, ++b)
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
                    if (vDmp[v] != 0)
                    {
                        pS1[b] *= dmC; pS2[b] *= dmC; pE[b] *= dmC;
                    }
                }
            }
            if (vBuzz[v] > 0.0001)
            {
                vBph[v] += 2 * vF[v] / sr;
                vBph[v] -= std::floor(vBph[v]);
                y += vBuzz[v] * 0.25 * (vBph[v] < 0.5 ? 1 : -1) * std::fabs(pS1[v * NM]);
                vBuzz[v] *= bzC;
            }
            vLf[v] = life;
            sl += y * vGl[v];
            sR += y * vGr[v];
            if (life < 0.00002)
            {
                vNote[v] = -1;
                for (int k = 0, b = v * NM; k < NM; ++k, ++b)
                    pS1[b] = pS2[b] = pE[b] = 0;
                vBuzz[v] = 0;
            }
        }

        const double bxl = bl[0].tick(sl) * 0.6 + bl[1].tick(sl) * 0.5 + bl[2].tick(sl) * 0.35 + bl[3].tick(sl) * 0.2;
        const double bxr = br[0].tick(sR) * 0.6 + br[1].tick(sR) * 0.5 + br[2].tick(sR) * 0.35 + br[3].tick(sR) * 0.2;

        seedC = pm(seedC);
        seedW = pm(seedW);
        const double c = hc.tick((seedC / 1073741824 - 1) * ck);
        ck *= ckC;
        if (ck < 0.0000001) ck = 0;
        whG = whC * whG + (1 - whC) * (stIdle < 1.5 * sr ? 1 : 0);
        gov += tpi * 23 * (1 - 0.5 * std::min(1.0, wdD / (3 * sr))) / sr;
        if (gov >= tpi) gov -= tpi;
        const double wh = hw.tick(seedW / 1073741824 - 1) * 0.004 * mech * (0.6 + 0.4 * std::sin(gov)) * whG;

        outL = (sl + bxl * body * 3 + c + wh) * 0.12 * vol;
        outR = (sR + bxr * body * 3 + c * 0.9 + wh) * 0.12 * vol;
    }

    static constexpr double tpi = 2 * kPi;

    double fSr = 44100;
    double vNote[NV] = {}, vF[NV] = {}, vBuzz[NV] = {}, vBph[NV] = {}, vGl[NV] = {}, vGr[NV] = {}, vDmp[NV] = {}, vLf[NV] = {};
    double pS1[NV * NM] = {}, pS2[NV * NM] = {}, pCf[NV * NM] = {}, pDf[NV * NM] = {}, pE[NV * NM] = {}, pR[NV * NM] = {};
    double wq[kQueue][3] = {};
    int wqN = 0;
    double wdD = 0, wdIdle = 0, cnt = 0, stIdle = 0;
    bool wdDead = false;
    double ck = 0, gov = 0, whG = 0, seedC = 12345, seedW = 67890;
    double ckC = 0, bzC = 0, dmC = 0, whC = 0;
    Biquad bl[4], br[4], hc, hw;
    double dsc = 1, bri = 1, body = 0.5, mech = 0.3, wdAmt = 0, vs = 0.6, wd = 0.5, vol = 1;

    MidiEv mq[kMaxEvents];
    uint32_t mqN = 0, mqI = 0;
    double si = 0;
};

} // namespace nullmusicbox
