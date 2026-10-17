/*
 * NULL JSFX - Drum Machine, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/drum_machine.jsfx, computed in double like EEL2 and in the
 * same order: ten synthesized voices, the 16-step sequencer that follows the host's transport,
 * and MIDI notes on the General MIDI drum map. Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's note-ons, and process.
 */

#pragma once

#include "DrumMachinePatterns.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nulldrum {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 56;
static constexpr int kMaxNotes = 128;

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

// band-limited square (PolyBLEP) at phase p with phase step dt
inline double square(double p, double dt)
{
    double q = p + 0.5;
    q -= std::floor(q);
    return (p < 0.5 ? 1 : -1) + blep(p, dt) - blep(q, dt);
}

// the TR-808 metal: six squares
struct Metal {
    double d1 = 0, d2 = 0, d3 = 0, d4 = 0, d5 = 0, d6 = 0;
    double p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0;

    void set(double r, double sr)
    {
        d1 = 205.3 * r / sr; d2 = 304.4 * r / sr; d3 = 369.6 * r / sr;
        d4 = 522.7 * r / sr; d5 = 540 * r / sr; d6 = 800 * r / sr;
    }
    void reset() { p1 = 0; p2 = 0.13; p3 = 0.29; p4 = 0.41; p5 = 0.57; p6 = 0.73; }
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

struct Tom {
    double f0 = 0, sw = 0, pc = 0, ac = 0, nc = 0;
    double ph = 0, env = 0, pe = 0, ne = 0;
    bool on = false;

    void trig(double v) { ph = 0; env = v; pe = 1; ne = v; on = true; }
    double run(double n, double sr)
    {
        double y = 0;
        if (on)
        {
            ph += f0 * (1 + sw * pe) / sr;
            ph -= std::floor(ph);
            y = std::sin(2 * kPi * ph) * env + n * 0.08 * ne;
            pe *= pc;
            env *= ac;
            ne *= nc;
            if (env < 0.000001)
                on = false;
        }
        return y;
    }
};

struct Note {
    uint32_t offset;
    int note;
    double vel;
};

class DrumMachine {
public:
    DrumMachine()
    {
        defaultUser(fUser);
        for (int k = 1; k <= kNumFactory; ++k)
            factory(k, fFactory[k - 1]);
        for (int i = 0; i < kNumSliders; ++i)
            fSl[i] = 0;
    }

    // @init: the sample rate, every voice quiet, the sequencer waiting for the next step
    void setSampleRate(double sr)
    {
        fSr = sr;
        reset();
    }

    // what @init resets (REAPER runs it again at every transport start)
    void reset()
    {
        kOn = sOn = cOn = rOn = hOn = oOn = bOn = yOn = false;
        lt.on = ht.on = false;
        std::fill(fPend, fPend + kTracks, 0.0);
        fLastS = -1000;
        fSeed = 22222;
        grPh = 0; grHl = grHr = 0;
    }

    // @slider, from the 56 slider values in order
    void setParams(const double* v)
    {
        std::copy(v, v + kNumSliders, fSl);
        const double sr = fSr;
        auto dcy = [sr](double tau) { return std::exp(-1 / (tau * sr)); };
        const double* s = fSl - 1; // s[n] = slider n

        fPat = std::floor(s[1] + 0.5);
        fSw = s[2] / 100;
        fAcc = s[3] / 100;
        fDrv = s[4] / 100;
        fDrvG = 1 + 3 * fDrv;
        fGrit = s[5] / 100;
        grStep = 48000 * std::pow(1.0 / 6, fGrit) / sr;
        grQ = std::pow(2.0, 15 - 11 * fGrit);
        fVol = std::pow(10.0, s[6] / 20);

        double r, d, o;
        panSet(0, s[7], s[11]);
        kF0 = 48 * std::pow(2.0, s[8] / 12);
        d = s[9] / 100; o = s[10] / 100;
        kSw = 1.5 + 4 * o;
        kPc = dcy(0.008 + 0.012 * (1 - o));
        kAc = dcy(0.06 + 1.4 * d * d);
        kCc = dcy(0.0015);
        kCl = 0.6 * o;

        panSet(1, s[12], s[16]);
        r = std::pow(2.0, s[13] / 12);
        d = s[14] / 100; o = s[15] / 100;
        sF1 = 185 * r; sF2 = 330 * r;
        sPc = dcy(0.012);
        sBc = dcy(0.02 + 0.12 * d);
        sNc = dcy(0.03 + 0.05 * d + 0.25 * d * d);
        sBg = 1 - 0.7 * o; sNg = 0.3 + 0.7 * o;
        sHp.set(1800 * r, 0.7, sr);

        panSet(2, s[17], s[21]);
        r = std::pow(2.0, s[18] / 12);
        d = s[19] / 100; o = s[20] / 100;
        cBp.set(1100 * r, 1.6, sr);
        cPer = std::floor(0.011 * sr);
        cBc = dcy(0.0045);
        cTc = dcy(0.05 + 0.05 * d + 0.5 * d * d);
        cTail = 0.25 + 0.75 * o;

        panSet(3, s[22], s[26]);
        r = std::pow(2.0, s[23] / 12);
        d = s[24] / 100; o = s[25] / 100;
        rD1 = 480 * r / sr; rD2 = 1650 * r / sr; rD3 = 2500 * r / sr;
        rC = dcy(0.006 + 0.06 * d);
        rM = o;

        panSet(4, s[27], s[31]);
        r = std::pow(2.0, s[28] / 12);
        d = s[29] / 100;
        hMx = s[30] / 100;
        hM.set(r, sr);
        hBp.set(9000 * r, 1.2, sr);
        hHp.set(6500 * r, 0.7, sr);
        hC = dcy(0.008 + 0.07 * d);

        panSet(5, s[32], s[36]);
        r = std::pow(2.0, s[33] / 12);
        d = s[34] / 100;
        oMx = s[35] / 100;
        oM.set(r, sr);
        oBp.set(9000 * r, 1.2, sr);
        oHp.set(6500 * r, 0.7, sr);
        oC = dcy(0.06 + 0.05 * d + 0.9 * d * d);
        oKc = dcy(0.004);

        panSet(6, s[37], s[41]);
        tomSet(lt, 95 * std::pow(2.0, s[38] / 12), s[39] / 100, s[40] / 100);
        panSet(7, s[42], s[46]);
        tomSet(ht, 165 * std::pow(2.0, s[43] / 12), s[44] / 100, s[45] / 100);

        panSet(8, s[47], s[51]);
        r = std::pow(2.0, s[48] / 12);
        d = s[49] / 100; o = s[50] / 100;
        bD1 = 540 * r / sr; bD2 = 800 * r / sr;
        bBp.set((600 + 1800 * o) * r, 2.5, sr);
        bC1 = dcy(0.012);
        bC2 = dcy(0.04 + 0.4 * d);

        panSet(9, s[52], s[56]);
        r = std::pow(2.0, s[53] / 12);
        d = s[54] / 100; o = s[55] / 100;
        yM.set(r * 1.18, sr);
        yBp.set((5000 + 4000 * o) * r, 0.8, sr);
        yHp.set(4000 * r, 0.7, sr);
        yC = dcy(0.25 + 0.2 * d + 2.2 * d * d);
        yMx = 0.15 + 0.6 * o;
    }

    // the User pattern (kPatternSize values)
    double* user() { return fUser; }
    const double* user() const { return fUser; }

    // @block: the note-ons of the block (offsets within it, velocity 0..1), the transport
    void beginBlock(const Note* notes, uint32_t count, bool playing, double tempo, double beatPos)
    {
        fNumNotes = std::min<uint32_t>(count, kMaxNotes);
        std::copy(notes, notes + fNumNotes, fNotes);
        fNoteIdx = 0;
        fSeqOn = playing && fPat > 0;
        if (!fSeqOn)
            fLastS = -1000;
        fPb = fPat <= 1 ? fUser : fFactory[int(fPat) - 2];
        fInc = tempo / (60 * fSr);
        fB0 = beatPos;
        fFlamN = std::max(1.0, std::floor(0.018 * fSr));
        fRollN = std::max(1.0, std::floor(7.5 / tempo * fSr));
        fSi = 0;
    }

    void trig(int t, double v)
    {
        ++hits[t];
        switch (t)
        {
        case 0: kPh = 0; kEnv = v; kPe = 1; kCe = v; kOn = true; break;
        case 1: sPh1 = 0; sPh2 = 0; sPe = 1; sBe = v; sNe = v; sOn = true; break;
        case 2: cT = 0; cEnv = 1; cVel = v; cOn = true; break;
        case 3: rPh1 = 0; rPh2 = 0; rPh3 = 0; rEnv = v; rOn = true; break;
        case 4: hM.reset(); hEnv = v; hOn = true; oChoke = true; break;
        case 5: oM.reset(); oEnv = v; oOn = true; oChoke = false; break;
        case 6: lt.trig(v); break;
        case 7: ht.trig(v); break;
        case 8: bP1 = 0; bP2 = 0.25; bE1 = 0.7 * v; bE2 = 0.3 * v; bOn = true; break;
        case 9: yM.reset(); yEnv = v; yOn = true; break;
        default: break;
        }
    }

    // General MIDI drum notes to voices
    void noteOn(int n, double v)
    {
        if (n == 35 || n == 36) trig(0, v);
        else if (n == 38 || n == 40) trig(1, v);
        else if (n == 39) trig(2, v);
        else if (n == 37) trig(3, v);
        else if (n == 42 || n == 44) trig(4, v);
        else if (n == 46) trig(5, v);
        else if (n == 41 || n == 43 || n == 45) trig(6, v);
        else if (n == 47 || n == 48 || n == 50) trig(7, v);
        else if (n == 56) trig(8, v);
        else if (n == 49 || n == 51 || n == 52 || n == 55 || n == 57 || n == 59) trig(9, v);
    }

    template <typename T>
    void process(T* outL, T* outR, uint32_t frames)
    {
        const double sr = fSr;
        for (uint32_t i = 0; i < frames; ++i)
        {
            while (fNoteIdx < fNumNotes && fNotes[fNoteIdx].offset <= fSi)
            {
                noteOn(fNotes[fNoteIdx].note, fNotes[fNoteIdx].vel);
                ++fNoteIdx;
            }

            // delayed hits (flam, roll)
            for (int t = 0; t < kTracks; ++t)
                if (fPend[t] > 0)
                {
                    fPend[t] -= 1;
                    if (fPend[t] < 0.5)
                        trig(t, fPVel[t]);
                }

            // sixteenths, the odd ones moved later by Swing
            if (fSeqOn)
            {
                const double bb = fB0 + fSi * fInc;
                if (bb >= 0)
                {
                    const double p8 = std::floor(bb * 2 + 0.000001);
                    const double s = p8 * 2 + (bb * 2 - p8 + 0.000001 >= fSw ? 1 : 0);
                    if (s != fLastS)
                    {
                        const bool odd = (int64_t(s) & 1) != 0;
                        if (fLastS > -1000 || bb - (odd ? p8 + fSw : p8) / 2 < 0.001)
                            playStep(s);
                        fLastS = s;
                    }
                }
            }
            step = fSeqOn ? fLastS : -1;
            fSi += 1;

            fSeed *= 16807;
            fSeed -= std::floor(fSeed / 2147483647) * 2147483647;
            const double nz = fSeed / 1073741824 - 1;
            double outl = 0, outr = 0, y;

            if (kOn)
            {
                kPh += kF0 * (1 + kSw * kPe) / sr;
                kPh -= std::floor(kPh);
                y = std::sin(2 * kPi * kPh) * kEnv + nz * kCe * kCl;
                kPe *= kPc; kEnv *= kAc; kCe *= kCc;
                if (kEnv < 0.000001) kOn = false;
                y *= 0.45 * fVl[0]; outl += y * fVpl[0]; outr += y * fVpr[0];
            }

            if (sOn)
            {
                sPh1 += sF1 * (1 + 0.6 * sPe) / sr; sPh1 -= std::floor(sPh1);
                sPh2 += sF2 * (1 + 0.6 * sPe) / sr; sPh2 -= std::floor(sPh2);
                sHp.run(nz);
                y = (std::sin(2 * kPi * sPh1) + 0.6 * std::sin(2 * kPi * sPh2)) * 0.6 * sBe * sBg + sHp.hp * sNe * sNg;
                sPe *= sPc; sBe *= sBc; sNe *= sNc;
                if (sBe + sNe < 0.000001) sOn = false;
                y *= 0.35 * fVl[1]; outl += y * fVpl[1]; outr += y * fVpr[1];
            }

            if (cOn)
            {
                double g;
                if (cT < 3 * cPer)
                {
                    if (cT == 0 || cT == cPer || cT == 2 * cPer) cEnv = 1;
                    g = cEnv; cEnv *= cBc;
                }
                else
                {
                    if (cT == 3 * cPer) cEnv = cTail;
                    g = cEnv; cEnv *= cTc;
                    if (cEnv < 0.000001) cOn = false;
                }
                cT += 1;
                cBp.run(nz);
                y = cBp.bp * g * cVel * 1.2 * fVl[2]; outl += y * fVpl[2]; outr += y * fVpr[2];
            }

            if (rOn)
            {
                rPh1 += rD1; rPh1 -= std::floor(rPh1);
                rPh2 += rD2; rPh2 -= std::floor(rPh2);
                rPh3 += rD3; rPh3 -= std::floor(rPh3);
                y = ((1 - rM) * (0.6 * std::sin(2 * kPi * rPh1) + 0.5 * std::sin(2 * kPi * rPh2) + 0.4 * nz * rEnv)
                     + rM * std::sin(2 * kPi * rPh3)) * rEnv;
                rEnv *= rC;
                if (rEnv < 0.000001) rOn = false;
                y *= 0.24 * fVl[3]; outl += y * fVpl[3]; outr += y * fVpr[3];
            }

            if (hOn)
            {
                hBp.run((1 - hMx) * hM.run() + hMx * nz);
                hHp.run(hBp.bp);
                y = hHp.hp * hEnv;
                hEnv *= hC;
                if (hEnv < 0.000001) hOn = false;
                y *= 0.68 * fVl[4]; outl += y * fVpl[4]; outr += y * fVpr[4];
            }

            if (oOn)
            {
                oBp.run((1 - oMx) * oM.run() + oMx * nz);
                oHp.run(oBp.bp);
                y = oHp.hp * oEnv;
                oEnv *= oChoke ? oKc : oC;
                if (oEnv < 0.000001) oOn = false;
                y *= 0.68 * fVl[5]; outl += y * fVpl[5]; outr += y * fVpr[5];
            }

            if (lt.on) { y = lt.run(nz, sr) * 0.38 * fVl[6]; outl += y * fVpl[6]; outr += y * fVpr[6]; }
            if (ht.on) { y = ht.run(nz, sr) * 0.38 * fVl[7]; outl += y * fVpl[7]; outr += y * fVpr[7]; }

            if (bOn)
            {
                bBp.run(square(bP1, bD1) + square(bP2, bD2));
                bP1 += bD1; bP1 -= std::floor(bP1);
                bP2 += bD2; bP2 -= std::floor(bP2);
                y = bBp.bp * (bE1 + bE2);
                bE1 *= bC1; bE2 *= bC2;
                if (bE2 < 0.000001) bOn = false;
                y *= 0.13 * fVl[8]; outl += y * fVpl[8]; outr += y * fVpr[8];
            }

            if (yOn)
            {
                yBp.run((1 - yMx) * yM.run() + yMx * nz);
                yHp.run(yBp.bp);
                y = yHp.hp * yEnv;
                yEnv *= yC;
                if (yEnv < 0.000001) yOn = false;
                y *= 0.6 * fVl[9]; outl += y * fVpl[9]; outr += y * fVpr[9];
            }

            // Drive: soft clipping of the whole kit
            if (fDrv > 0)
            {
                double u = std::max(-3.0, std::min(3.0, outl * fDrvG));
                outl = u * (27 + u * u) / (27 + 9 * u * u) / (1 + fDrv);
                u = std::max(-3.0, std::min(3.0, outr * fDrvG));
                outr = u * (27 + u * u) / (27 + 9 * u * u) / (1 + fDrv);
            }

            // Grit: sample-and-hold at a lower rate and fewer bits
            if (fGrit > 0)
            {
                grPh += grStep;
                if (grPh >= 1)
                {
                    grPh -= std::floor(grPh);
                    grHl = std::floor(outl * grQ + 0.5) / grQ;
                    grHr = std::floor(outr * grQ + 0.5) / grQ;
                }
                outl = grHl; outr = grHr;
            }

            outL[i] = static_cast<T>(outl * fVol);
            outR[i] = static_cast<T>(outr * fVol);
        }
    }

    // for the interface: the step that plays (-1 when stopped), a count of each voice's hits
    double step = -1;
    uint32_t hits[kTracks] = {};

private:
    void panSet(int t, double lev, double p)
    {
        fVl[t] = std::pow(10.0, lev / 20);
        fVpl[t] = std::sqrt(2.0) * std::cos((p / 100 + 1) * kPi / 4);
        fVpr[t] = std::sqrt(2.0) * std::sin((p / 100 + 1) * kPi / 4);
    }

    void tomSet(Tom& tm, double f, double d, double o)
    {
        const double sr = fSr;
        auto dcy = [sr](double tau) { return std::exp(-1 / (tau * sr)); };
        tm.f0 = f;
        tm.sw = 0.15 + 1.6 * o;
        tm.pc = dcy(0.03 + 0.12 * o);
        tm.ac = dcy(0.05 + 0.6 * d * d + 0.1 * d);
        tm.nc = dcy(0.004);
    }

    // one sequencer step on every track: its value in the track's own length
    void playStep(double s)
    {
        for (int t = 0; t < kTracks; ++t)
        {
            const double len = std::max(1.0, fPb[kTracks * kSteps + t]);
            const double i = s - std::floor(s / len) * len;
            const double v = fPb[t * kSteps + int(i)];
            if (v > 0)
            {
                const double vel = v == 2 ? 1 : 1 - 0.5 * fAcc;
                if (v == 3) { trig(t, vel * 0.5); fPend[t] = fFlamN; fPVel[t] = vel; }
                else if (v == 4) { trig(t, vel); fPend[t] = fRollN; fPVel[t] = vel * 0.8; }
                else trig(t, vel);
            }
        }
    }

    double fSr = 48000;
    double fSl[kNumSliders];
    double fUser[kPatternSize];
    double fFactory[kNumFactory][kPatternSize];

    // @slider
    double fPat = 2, fSw = 0.5, fAcc = 0.5, fDrv = 0, fDrvG = 1, fGrit = 0, grStep = 1, grQ = 1, fVol = 1;
    double fVl[kTracks] = {}, fVpl[kTracks] = {}, fVpr[kTracks] = {};

    // @block and the sequencer
    Note fNotes[kMaxNotes];
    uint32_t fNumNotes = 0, fNoteIdx = 0, fSi = 0;
    bool fSeqOn = false;
    const double* fPb = fUser;
    double fInc = 0, fB0 = 0, fFlamN = 1, fRollN = 1, fLastS = -1000;
    double fPend[kTracks] = {}, fPVel[kTracks] = {};
    double fSeed = 22222;
    double grPh = 0, grHl = 0, grHr = 0;

    // voices (JSFX names without the underscore)
    bool kOn = false, sOn = false, cOn = false, rOn = false, hOn = false, oOn = false, bOn = false, yOn = false;
    double kF0 = 0, kSw = 0, kPc = 0, kAc = 0, kCc = 0, kCl = 0, kPh = 0, kEnv = 0, kPe = 0, kCe = 0;
    double sF1 = 0, sF2 = 0, sPc = 0, sBc = 0, sNc = 0, sBg = 0, sNg = 0, sPh1 = 0, sPh2 = 0, sPe = 0, sBe = 0, sNe = 0;
    Svf sHp;
    double cPer = 0, cBc = 0, cTc = 0, cTail = 0, cT = 0, cEnv = 0, cVel = 0;
    Svf cBp;
    double rD1 = 0, rD2 = 0, rD3 = 0, rC = 0, rM = 0, rPh1 = 0, rPh2 = 0, rPh3 = 0, rEnv = 0;
    double hMx = 0, hC = 0, hEnv = 0;
    Metal hM;
    Svf hBp, hHp;
    double oMx = 0, oC = 0, oKc = 0, oEnv = 0;
    bool oChoke = false;
    Metal oM;
    Svf oBp, oHp;
    Tom lt, ht;
    double bD1 = 0, bD2 = 0, bC1 = 0, bC2 = 0, bP1 = 0, bP2 = 0, bE1 = 0, bE2 = 0;
    Svf bBp;
    double yMx = 0, yC = 0, yEnv = 0;
    Metal yM;
    Svf yBp, yHp;
};

} // namespace nulldrum
