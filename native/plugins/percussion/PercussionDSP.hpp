/*
 * NULL JSFX - Percussion, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/percussion.jsfx, computed in double like EEL2 and in the
 * same order: eleven synthesized hand-percussion voices, the groove player that follows the
 * host's transport (or the keys C1..G1), the room. Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI, and process.
 */

#pragma once

#include "PercussionGrooves.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullperc {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 52;
static constexpr int kMaxEvents = 256;

struct MidiEv {
    uint32_t offset;
    uint8_t st, d1, d2;
};

// state-variable filter (TPT), as svf_set / svf in the JSFX
struct Svf {
    double g = 0, k = 0, a1 = 0, a2 = 0, a3 = 0, s1 = 0, s2 = 0, lp = 0, bp = 0, hp = 0;

    void set(double fc, double q, double sr)
    {
        g = std::tan(kPi * std::min(fc, 0.45 * sr) / sr);
        k = 1 / q;
        a1 = 1 / (1 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void run(double x)
    {
        const double v3 = x - s2;
        const double v1 = a1 * s1 + a2 * v3;
        const double v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2 * v1 - s1;
        s2 = 2 * v2 - s2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
};

inline double blep(double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1;
    }
    if (t > 1 - dt)
    {
        t = (t - 1) / dt;
        return t * t + t + t + 1;
    }
    return 0;
}

inline double square(double p, double dt)
{
    double q = p + 0.5;
    q -= std::floor(q);
    return (p < 0.5 ? 1 : -1) + blep(p, dt) - blep(q, dt);
}

// six squares at 205.3 .. 800 Hz times r (metal_set / metal)
struct Metal {
    double d1 = 0, d2 = 0, d3 = 0, d4 = 0, d5 = 0, d6 = 0;
    double p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0;

    void set(double r, double sr)
    {
        d1 = 205.3 * r / sr; d2 = 304.4 * r / sr; d3 = 369.6 * r / sr;
        d4 = 522.7 * r / sr; d5 = 540 * r / sr; d6 = 800 * r / sr;
    }
    double run()
    {
        const double y = square(p1, d1) + square(p2, d2) + square(p3, d3)
                       + square(p4, d4) + square(p5, d5) + square(p6, d6);
        p1 += d1; p1 -= std::floor(p1);
        p2 += d2; p2 -= std::floor(p2);
        p3 += d3; p3 -= std::floor(p3);
        p4 += d4; p4 -= std::floor(p4);
        p5 += d5; p5 -= std::floor(p5);
        p6 += d6; p6 -= std::floor(p6);
        return y / 6;
    }
};

// membrane (mem_set / mem_trig / mem)
struct Membrane {
    double f0 = 0, o1 = 0, o2 = 0, o3 = 0, m1 = 0, m2 = 0, m3 = 0, q1 = 0, q2 = 0, q3 = 0;
    double cp = 0, cno = 0, cns = 0, lao = 0, lbo = 0, las = 0, lbs = 0;
    double p1 = 0, p2 = 0, p3 = 0, pe = 0, e1 = 0, e2 = 0, e3 = 0, ne = 0;
    double c1 = 0, c2 = 0, c3 = 0, cn = 0, pa = 0, la = 0, lb = 0, na = 0, nb = 0;
    bool on = false;

    void set(double f, double d, double ta, double tb, double sr)
    {
        auto dcy = [sr](double tau) { return std::exp(-1 / (tau * sr)); };
        auto onep = [sr](double fc) { return 1 - std::exp(-2 * kPi * std::min(fc, 0.45 * sr) / sr); };
        f0 = f;
        double t = ta + tb * d * d;
        o1 = dcy(t); o2 = dcy(t * 0.5); o3 = dcy(t * 0.25);
        t = 0.016 + 0.024 * d;
        m1 = dcy(t); m2 = dcy(t * 0.6); m3 = dcy(t * 0.4);
        t = 0.02 + 0.03 * d;
        q1 = dcy(t); q2 = dcy(t * 0.8); q3 = dcy(t * 0.6);
        cp = dcy(0.012);
        cno = dcy(0.004);
        cns = dcy(0.01);
        lao = onep(f * 9); lbo = onep(f * 2.5);
        las = onep(7000); lbs = onep(1200);
    }
    void trig(int art, double v)
    {
        if (!on)
        {
            p1 = 0; p2 = 0; p3 = 0;
        }
        pe = 1;
        if (art == 0)
        {
            e1 = v; e2 = 0.45 * v; e3 = 0.2 * v; ne = 0.3 * v;
            c1 = o1; c2 = o2; c3 = o3; cn = cno;
            pa = 0.07; la = lao; lb = lbo;
        }
        else if (art == 1)
        {
            e1 = 0.75 * v; e2 = 0.15 * v; e3 = 0.05 * v; ne = 0.45 * v;
            c1 = m1; c2 = m2; c3 = m3; cn = cno;
            pa = 0.03; la = lao; lb = lbo;
        }
        else
        {
            e1 = 0.4 * v; e2 = 0.45 * v; e3 = 0.4 * v; ne = 1.2 * v;
            c1 = q1; c2 = q2; c3 = q3; cn = cns;
            pa = 0.12; la = las; lb = lbs;
        }
        on = true;
    }
    double run(double n, double sr)
    {
        const double fq = f0 * (1 + pa * pe);
        double q = fq / sr;
        p1 += q; p1 -= std::floor(p1);
        q = fq * 1.59;
        q = q / sr;
        p2 += q; p2 -= std::floor(p2);
        q = fq * 2.24;
        q = q / sr;
        p3 += q; p3 -= std::floor(p3);
        na += la * (n - na);
        nb += lb * (n - nb);
        const double y = std::sin(2 * kPi * p1) * e1 + std::sin(2 * kPi * p2) * e2
                       + std::sin(2 * kPi * p3) * e3 + (na - nb) * ne;
        pe *= cp;
        e1 *= c1; e2 *= c2; e3 *= c3;
        ne *= cn;
        if (e1 + ne < 0.000001)
            on = false;
        return y;
    }
};

class Percussion {
public:
    Percussion()
    {
        for (int k = 1; k <= kNumGrooves; ++k)
            grooveSteps(k, fGroove[k - 1]);
        for (int i = 0; i < kNumSliders; ++i)
            fSl[i] = 0;
    }

    // @init: the sample rate, every voice quiet, the room empty
    void setSampleRate(double sr)
    {
        fSr = sr;
        fRMax = int(std::ceil(0.06 * sr)) + 4;
        fRoom.assign(size_t(12) * size_t(fRMax), 0.0);
        reset();
    }

    // what @init resets (REAPER runs it again at every transport start)
    void reset()
    {
        cg.on = tu.on = bh.on = bl.on = false;
        shOn = tbOn = cbOn = clOn = guOn = caOn = trOn = false;
        std::fill(fKh, fKh + 8, 0.0);
        fKGrv = 0;
        fLastS = -1000;
        fFb = 0;
        fSeed = 22222;
        fHx = 12345;
        std::fill(fRoom.begin(), fRoom.end(), 0.0);
        std::fill(fRp, fRp + 12, 0.0);
        std::fill(fRf, fRf + 12, 0.0);
    }

    // @slider, from the 52 slider values in order
    void setParams(const double* v)
    {
        std::copy(v, v + kNumSliders, fSl);
        const double sr = fSr;
        auto dcy = [sr](double tau) { return std::exp(-1 / (tau * sr)); };
        const double* s = fSl - 1; // s[n] = slider n

        fGSel = std::floor(s[1] + 0.5);
        fKeys = s[2] >= 0.5;
        if (!fKeys)
            keysOff();
        fSw = s[3] / 100;
        fAcc = s[4] / 100;
        fHum = s[5] / 100;
        fRoomAmt = s[6] / 100;
        rmWet = 0.1 * fRoomAmt;
        double x = s[7] / 100;
        rmFb = 0.6 + 0.28 * x;
        rmD = 0.35;
        const double sc = 0.5 + 0.8 * x;
        roomLen(0, 17.3, sc); roomLen(1, 21.1, sc); roomLen(2, 24.7, sc); roomLen(3, 28.9, sc);
        roomLen(4, 17.9, sc); roomLen(5, 21.8, sc); roomLen(6, 25.3, sc); roomLen(7, 29.6, sc);
        roomLen(8, 5.1, 1); roomLen(9, 1.7, 1); roomLen(10, 5.6, 1); roomLen(11, 1.9, 1);
        fVol = std::pow(10.0, s[8] / 20);

        panSet(0, s[9], s[12]);
        cg.set(245 * std::pow(2.0, s[10] / 12), s[11] / 100, 0.06, 0.45, sr);
        panSet(1, s[13], s[16]);
        tu.set(178 * std::pow(2.0, s[14] / 12), s[15] / 100, 0.08, 0.55, sr);
        panSet(2, s[17], s[20]);
        bh.set(470 * std::pow(2.0, s[18] / 12), s[19] / 100, 0.035, 0.25, sr);
        panSet(3, s[21], s[24]);
        bl.set(335 * std::pow(2.0, s[22] / 12), s[23] / 100, 0.045, 0.3, sr);

        double r, d;
        panSet(4, s[25], s[28]);
        r = std::pow(2.0, s[26] / 12);
        d = s[27] / 100;
        shBp.set(5200 * r, 0.9, sr);
        shHp.set(2800 * r, 0.7, sr);
        shCa = dcy(0.025 + 0.12 * d);
        shCb = dcy(0.008);

        panSet(5, s[29], s[32]);
        r = std::pow(2.0, s[30] / 12);
        d = s[31] / 100;
        tbM.set(7.3 * r, sr);
        tbBp.set(7600 * r, 1.2, sr);
        tbHp.set(4500 * r, 0.7, sr);
        tbPer = std::floor(0.008 * sr);
        tbBc = dcy(0.006);
        tbTc = dcy(0.05 + 0.3 * d);

        panSet(6, s[33], s[36]);
        r = std::pow(2.0, s[34] / 12);
        d = s[35] / 100;
        cbD1 = 587 * r / sr; cbD2 = 845 * r / sr;
        cbBp.set(1000 * r, 2.2, sr);
        cbC1 = dcy(0.01);
        cbC2 = dcy(0.05 + 0.35 * d);

        panSet(7, s[37], s[40]);
        r = std::pow(2.0, s[38] / 12);
        d = s[39] / 100;
        clD = 2450 * r / sr;
        clC = dcy(0.025 + 0.12 * d);
        clCc = dcy(0.0007);

        panSet(8, s[41], s[44]);
        r = std::pow(2.0, s[42] / 12);
        d = s[43] / 100;
        guD = 60 * r / sr;
        x = 0.5 + d;
        x *= sr;
        guShort = std::floor(0.085 * x);
        guLong = std::floor(0.28 * x);
        guBp.set(2600 * r, 1.6, sr);
        guTc = dcy(0.0015);

        panSet(9, s[45], s[48]);
        r = std::pow(2.0, s[46] / 12);
        d = s[47] / 100;
        caBp.set(8200 * r, 1.4, sr);
        caHp.set(6000 * r, 0.7, sr);
        caCa = dcy(0.02 + 0.12 * d);
        caCb = dcy(0.0015);

        panSet(10, s[49], s[52]);
        r = std::pow(2.0, s[50] / 12);
        d = s[51] / 100;
        const double trF = 1180 * r;
        trD1 = trF / sr;
        trD2 = trF * 2.76; trD2 = trD2 / sr;
        trD3 = trF * 5.4; trD3 = trD3 / sr;
        trD4 = trF * 8.93; trD4 = trD4 / sr;
        trG2 = trD2 < 0.45; trG3 = trD3 < 0.45; trG4 = trD4 < 0.45;
        const double t = 0.25 + 1.6 * d * d;
        trO1 = dcy(t); trO2 = dcy(t * 0.85); trO3 = dcy(t * 0.7); trO4 = dcy(t * 0.55);
        trMc = dcy(0.025);
        trCc = dcy(0.0008);
    }

    // @block: the MIDI of the block (offsets within it), the transport
    void beginBlock(const MidiEv* ev, uint32_t count, bool playing, double tempo, double beatPos)
    {
        fNumEv = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + fNumEv, fEv);
        fEvIdx = 0;
        fPlaying = playing;
        fInc = tempo / (60 * fSr);
        fB0 = beatPos;
        fSi = 0;
    }

    // t the voice, art the stroke, v the velocity 0..1
    void trig(int t, int art, double v)
    {
        ++hits[t];
        switch (t)
        {
        case 0: cg.trig(art, v); break;
        case 1: tu.trig(art, v); break;
        case 2: bh.trig(art, v); break;
        case 3: bl.trig(art, v); break;
        case 4: shEa = v; shEb = v; shOn = true; break;
        case 5: tbT = 0; tbEnv = 1; tbVel = v; tbOn = true; break;
        case 6:
            if (!cbOn) { cbP1 = 0; cbP2 = 0.25; }
            cbE1 = 0.7 * v; cbE2 = 0.3 * v; cbOn = true;
            break;
        case 7:
            if (!clOn) clPh = 0;
            clEnv = v; clCk = v; clOn = true;
            break;
        case 8: guT = 0; guLen = art == 1 ? guLong : guShort; guPh = 0; guTk = 0; guVel = v; guOn = true; break;
        case 9: caEa = v; caEb = v; caOn = true; break;
        case 10:
            if (!trOn) { trP1 = 0; trP2 = 0; trP3 = 0; trP4 = 0; }
            trE1 = 0.35 * v; trE2 = 0.6 * v; trE3 = 0.5 * v; trE4 = 0.35 * v; trCk = v;
            trMute = art == 1;
            trOn = true;
            break;
        default: break;
        }
    }

    template <typename T>
    void process(T* const* outs, uint32_t frames)
    {
        const double sr = fSr;
        for (uint32_t i = 0; i < frames; ++i)
        {
            while (fEvIdx < fNumEv && fEv[fEvIdx].offset <= fSi)
            {
                const MidiEv& e = fEv[fEvIdx];
                const int st = e.st & 0xF0, n = e.d1, v = e.d2;
                if (st == 0x90 && v > 0)
                    noteOn(n, v / 127.0);
                else if ((st == 0x80 || st == 0x90) && fKeys && n >= 24 && n <= 31)
                    keyOff(n - 24);
                else if (st == 0xB0 && (n == 120 || n == 123))
                    keysOff();
                ++fEvIdx;
            }

            // the groove
            const double gCur = fKeys ? fKGrv : fPlaying ? fGSel : 0;
            if (gCur > 0)
            {
                fPb = fGroove[int(gCur) - 1];
                fGLen = kGrooves[int(gCur) - 1].len;
                const int spb = kGrooves[int(gCur) - 1].spb;
                double bb;
                if (fPlaying)
                    bb = fB0 + fSi * fInc;
                else
                {
                    bb = fFb;
                    fFb += fInc;
                }
                if (bb >= 0)
                {
                    double s, s0;
                    if (spb == 3)
                    {
                        s = std::floor(bb * 3 + 0.000001);
                        s0 = s / 3;
                    }
                    else
                    {
                        const double p8 = std::floor(bb * 2 + 0.000001);
                        s = p8 * 2 + (bb * 2 - p8 + 0.000001 >= fSw ? 1 : 0);
                        s0 = ((int64_t(s) & 1) ? p8 + fSw : p8) / 2;
                    }
                    if (s != fLastS)
                    {
                        if (fLastS > -1000 || bb - s0 < 0.001)
                            playStep(s);
                        fLastS = s;
                    }
                }
            }
            else
                fLastS = -1000;
            step = gCur > 0 ? fLastS : -1;
            groove = gCur;
            fSi += 1;

            fSeed *= 16807;
            fSeed -= std::floor(fSeed / 2147483647) * 2147483647;
            const double nz = fSeed / 1073741824 - 1;
            double outl = 0, outr = 0, y;

            if (cg.on) { y = cg.run(nz, sr) * 0.357 * fVl[0]; outl += y * fVpl[0]; outr += y * fVpr[0]; }
            if (tu.on) { y = tu.run(nz, sr) * 0.383 * fVl[1]; outl += y * fVpl[1]; outr += y * fVpr[1]; }
            if (bh.on) { y = bh.run(nz, sr) * 0.306 * fVl[2]; outl += y * fVpl[2]; outr += y * fVpr[2]; }
            if (bl.on) { y = bl.run(nz, sr) * 0.323 * fVl[3]; outl += y * fVpl[3]; outr += y * fVpr[3]; }

            if (shOn)
            {
                shBp.run(nz);
                shHp.run(shBp.bp);
                y = shHp.hp * (shEa - shEb);
                shEa *= shCa; shEb *= shCb;
                if (shEa < 0.000001) shOn = false;
                y *= 0.9 * fVl[4]; outl += y * fVpl[4]; outr += y * fVpr[4];
            }

            if (tbOn)
            {
                double g;
                if (tbT < 3 * tbPer)
                {
                    if (tbT == 0 || tbT == tbPer || tbT == 2 * tbPer)
                        tbEnv = tbT == 0 ? 1 : tbT == tbPer ? 0.75 : 0.5;
                    g = tbEnv; tbEnv *= tbBc;
                }
                else
                {
                    if (tbT == 3 * tbPer) tbEnv = 0.45;
                    g = tbEnv; tbEnv *= tbTc;
                    if (tbEnv < 0.000001) tbOn = false;
                }
                tbT += 1;
                tbBp.run(0.6 * tbM.run() + 0.4 * nz);
                tbHp.run(tbBp.bp);
                y = tbHp.hp * g * tbVel;
                y *= 0.95 * fVl[5]; outl += y * fVpl[5]; outr += y * fVpr[5];
            }

            if (cbOn)
            {
                cbBp.run(square(cbP1, cbD1) + square(cbP2, cbD2));
                cbP1 += cbD1; cbP1 -= std::floor(cbP1);
                cbP2 += cbD2; cbP2 -= std::floor(cbP2);
                y = cbBp.bp * (cbE1 + cbE2);
                cbE1 *= cbC1; cbE2 *= cbC2;
                if (cbE2 < 0.000001) cbOn = false;
                y *= 0.18 * fVl[6]; outl += y * fVpl[6]; outr += y * fVpr[6];
            }

            if (clOn)
            {
                clPh += clD; clPh -= std::floor(clPh);
                y = std::sin(2 * kPi * clPh) * clEnv + nz * clCk * 0.3;
                clEnv *= clC; clCk *= clCc;
                if (clEnv < 0.000001) clOn = false;
                y *= 0.36 * fVl[7]; outl += y * fVpl[7]; outr += y * fVpr[7];
            }

            if (guOn)
            {
                guPh += guD;
                if (guPh >= 1) { guPh -= 1; guTk = 1; }
                guBp.run(nz * (guTk + 0.12));
                const double x = guT / guLen;
                y = guBp.bp * std::sin(kPi * x) * guVel;
                guTk *= guTc;
                guT += 1;
                if (guT >= guLen) guOn = false;
                y *= 0.7 * fVl[8]; outl += y * fVpl[8]; outr += y * fVpr[8];
            }

            if (caOn)
            {
                caBp.run(nz);
                caHp.run(caBp.bp);
                y = caHp.hp * (caEa - caEb);
                caEa *= caCa; caEb *= caCb;
                if (caEa < 0.000001) caOn = false;
                y *= 0.6 * fVl[9]; outl += y * fVpl[9]; outr += y * fVpr[9];
            }

            if (trOn)
            {
                trP1 += trD1; trP1 -= std::floor(trP1);
                trP2 += trD2; trP2 -= std::floor(trP2);
                trP3 += trD3; trP3 -= std::floor(trP3);
                trP4 += trD4; trP4 -= std::floor(trP4);
                y = std::sin(2 * kPi * trP1) * trE1 + trG2 * std::sin(2 * kPi * trP2) * trE2
                  + trG3 * std::sin(2 * kPi * trP3) * trE3 + trG4 * std::sin(2 * kPi * trP4) * trE4 + nz * trCk * 0.2;
                if (trMute)
                {
                    trE1 *= trMc; trE2 *= trMc; trE3 *= trMc; trE4 *= trMc;
                }
                else
                {
                    trE1 *= trO1; trE2 *= trO2; trE3 *= trO3; trE4 *= trO4;
                }
                trCk *= trCc;
                if (trE1 + trE2 + trE3 + trE4 < 0.000001) trOn = false;
                y *= 0.3 * fVl[10]; outl += y * fVpl[10]; outr += y * fVpr[10];
            }

            // Room
            if (fRoomAmt > 0)
            {
                double wl = comb(0, outl);
                wl += comb(1, outl);
                wl += comb(2, outl);
                wl += comb(3, outl);
                double wr = comb(4, outr);
                wr += comb(5, outr);
                wr += comb(6, outr);
                wr += comb(7, outr);
                wl = allpass(9, allpass(8, wl));
                wr = allpass(11, allpass(10, wr));
                outl += wl * rmWet;
                outr += wr * rmWet;
            }

            outs[0][i] = static_cast<T>(outl * fVol);
            outs[1][i] = static_cast<T>(outr * fVol);
        }
    }

    // for the interface: the groove that plays (0 none), its step (-1 when none), the hits
    double step = -1, groove = 0;
    uint32_t hits[kTracks] = {};

private:
    void panSet(int t, double lev, double p)
    {
        fVl[t] = std::pow(10.0, lev / 20);
        fVpl[t] = std::sqrt(2.0) * std::cos((p / 100 + 1) * kPi / 4);
        fVpr[t] = std::sqrt(2.0) * std::sin((p / 100 + 1) * kPi / 4);
    }

    static double pm(double x)
    {
        x *= 16807;
        return x - std::floor(x / 2147483647) * 2147483647;
    }

    void noteOn(int n, double v)
    {
        if (fKeys && n >= 24 && n <= 31)
        {
            if (fKGrv == 0 && !fPlaying)
            {
                fFb = 0;
                fLastS = -1000;
            }
            fKh[n - 24] = 1;
            fKGrv = n - 23;
        }
        else if (n == 60) trig(2, 0, v);
        else if (n == 61) trig(3, 0, v);
        else if (n == 62) trig(0, 1, v);
        else if (n == 63) trig(0, 0, v);
        else if (n == 64) trig(1, 0, v);
        else if (n == 65) trig(0, 2, v);
        else if (n == 66) trig(1, 2, v);
        else if (n == 54) trig(5, 0, v);
        else if (n == 56) trig(6, 0, v);
        else if (n == 69) trig(9, 0, v);
        else if (n == 70 || n == 82) trig(4, 0, v);
        else if (n == 73) trig(8, 0, v);
        else if (n == 74) trig(8, 1, v);
        else if (n == 75) trig(7, 0, v);
        else if (n == 80) trig(10, 1, v);
        else if (n == 81) trig(10, 0, v);
    }

    void keyOff(int k)
    {
        fKh[k] = 0;
        if (std::fabs(fKGrv - (k + 1)) < 0.00001)
        {
            fKGrv = 0;
            for (int i = 0; i < 8; ++i)
                if (fKh[i] != 0)
                    fKGrv = i + 1;
        }
    }

    void keysOff()
    {
        std::fill(fKh, fKh + 8, 0.0);
        fKGrv = 0;
    }

    void playStep(double s)
    {
        const double i = s - std::floor(s / fGLen) * fGLen;
        for (int t = 0; t < kTracks; ++t)
        {
            const double v = fPb[t * kMaxSteps + int(i)];
            if (v > 0)
            {
                double vel = v == 2 ? 0.6 : v == 3 ? 0.24 * (1 - 0.5 * fAcc) : 0.6 - 0.3 * fAcc;
                if (fHum > 0)
                {
                    fHx = pm(fHx);
                    double hr = fHx / 2147483647;
                    hr *= fHum;
                    vel *= 1 - 0.5 * hr;
                }
                const int art = v == 4 || v == 6 ? 1 : v == 5 ? 2 : 0;
                trig(t, art, vel);
            }
        }
    }

    void roomLen(int j, double ms, double sc)
    {
        double x = ms * sc;
        x = x * fSr;
        fRl[j] = std::max(1.0, std::min(double(fRMax - 1), std::floor(x / 1000)));
        if (fRp[j] >= fRl[j])
            fRp[j] = 0;
    }

    double comb(int j, double x)
    {
        double* b = fRoom.data() + size_t(j) * size_t(fRMax);
        const int p = int(fRp[j]);
        const double y = b[p];
        fRf[j] = y + rmD * (fRf[j] - y);
        b[p] = x + fRf[j] * rmFb;
        fRp[j] += 1;
        if (fRp[j] >= fRl[j])
            fRp[j] = 0;
        return y;
    }

    double allpass(int j, double x)
    {
        double* b = fRoom.data() + size_t(j) * size_t(fRMax);
        const int p = int(fRp[j]);
        const double y = b[p];
        const double o = y - 0.5 * x;
        b[p] = x + 0.5 * o;
        fRp[j] += 1;
        if (fRp[j] >= fRl[j])
            fRp[j] = 0;
        return o;
    }

    double fSr = 48000;
    double fSl[kNumSliders];
    double fGroove[kNumGrooves][kTracks * kMaxSteps];

    // @slider
    double fGSel = 1, fSw = 0.5, fAcc = 0.5, fHum = 0, fRoomAmt = 0, fVol = 1;
    bool fKeys = false;
    double rmWet = 0, rmFb = 0, rmD = 0.35;
    double fVl[kTracks] = {}, fVpl[kTracks] = {}, fVpr[kTracks] = {};

    // @block, the MIDI and the groove
    MidiEv fEv[kMaxEvents];
    uint32_t fNumEv = 0, fEvIdx = 0, fSi = 0;
    bool fPlaying = false;
    double fInc = 0, fB0 = 0, fLastS = -1000, fFb = 0, fGLen = 16;
    const double* fPb = fGroove[0];
    double fKh[8] = {}, fKGrv = 0;
    double fSeed = 22222, fHx = 12345;

    // the room
    int fRMax = 0;
    std::vector<double> fRoom;
    double fRl[12] = {}, fRp[12] = {}, fRf[12] = {};

    // voices (JSFX names without the underscore)
    Membrane cg, tu, bh, bl;
    bool shOn = false, tbOn = false, cbOn = false, clOn = false, guOn = false, caOn = false, trOn = false;
    Svf shBp, shHp;
    double shCa = 0, shCb = 0, shEa = 0, shEb = 0;
    Metal tbM;
    Svf tbBp, tbHp;
    double tbPer = 0, tbBc = 0, tbTc = 0, tbT = 0, tbEnv = 0, tbVel = 0;
    Svf cbBp;
    double cbD1 = 0, cbD2 = 0, cbC1 = 0, cbC2 = 0, cbP1 = 0, cbP2 = 0, cbE1 = 0, cbE2 = 0;
    double clD = 0, clC = 0, clCc = 0, clPh = 0, clEnv = 0, clCk = 0;
    Svf guBp;
    double guD = 0, guShort = 1, guLong = 1, guTc = 0, guT = 0, guLen = 1, guPh = 0, guTk = 0, guVel = 0;
    Svf caBp, caHp;
    double caCa = 0, caCb = 0, caEa = 0, caEb = 0;
    double trD1 = 0, trD2 = 0, trD3 = 0, trD4 = 0, trG2 = 0, trG3 = 0, trG4 = 0;
    double trO1 = 0, trO2 = 0, trO3 = 0, trO4 = 0, trMc = 0, trCc = 0;
    double trP1 = 0, trP2 = 0, trP3 = 0, trP4 = 0, trE1 = 0, trE2 = 0, trE3 = 0, trE4 = 0, trCk = 0;
    bool trMute = false;
};

} // namespace nullperc
