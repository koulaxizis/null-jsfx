/*
 * NULL JSFX - Bass, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/bass.jsfx, computed in double like EEL2 and in the same
 * order: one monophonic voice (Sub, Saw, Square, 808), the 24 dB low-pass, Drive, the ADSR and
 * Punch, last-note priority with glide, and the phrase player on the keys below C2. Comments
 * name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullbass {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 19;
static constexpr int kMaxEvents = 512;
static constexpr int kNumPhrases = 8;

// the phrases: steps, accents/slides ('a', 's') and gate, as in the JSFX's phr_set calls
struct PhraseDef {
    const char* notes;
    const char* mods;
    double gate;
};
static const PhraseDef kPhrases[kNumPhrases] = {
    { "r.r.r.r.r.r.r.r.", "a...a...a...a...", 0.7 },  // Eighths
    { "r.o.r.o.r.o.r.o.", "a.......a.......", 0.55 }, // Octaves
    { "..r...r...r...r.", "................", 0.6 },  // Offbeat
    { "r.rrr.rrr.rrr.rr", "a...a...a...a...", 0.65 }, // Gallop
    { "r..ro..rr.s.f.o.", "a...a.......a...", 0.45 }, // Funk
    { "rror.rfr.ormo.sr", "a.s..as..a.s..as", 0.5 },  // Acid
    { "r..r..r.r..f..o.", "a..a..a.a..a..a.", 0.6 },  // Tresillo
    { "r-------..r-o---", "a.........a.s...", 0.95 }, // Trap
};

inline double ival(char c)
{
    switch (c)
    {
    case 'r': return 0;
    case 'o': return 12;
    case 'f': return 7;
    case 'l': return -5;
    case 'd': return -12;
    case 'm': return 3;
    case 'q': return 5;
    case 's': return 10;
    case 'n': return 2;
    case '-': return -200;
    default: return -100;
    }
}

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

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

class Bass {
public:
    Bass()
    {
        for (int k = 1; k <= kNumPhrases; ++k)
        {
            const PhraseDef& d = kPhrases[k - 1];
            for (int i = 0; i < 16; ++i)
            {
                PN[k * 16 + i] = ival(d.notes[i]);
                PA[k * 16 + i] = d.mods[i] == 'a' ? 1 : 0;
                PS[k * 16 + i] = d.mods[i] == 's' ? 1 : 0;
            }
            PG[k] = d.gate;
        }
    }

    void setSampleRate(double sr)
    {
        fSr = sr;
        reset();
    }

    // what @init resets
    void reset()
    {
        vStage = 0; vEnv = 0; vGate = false; vHas = false; vCur = vTgt = 48; vGc = 1;
        vPh = vPh2 = vPhs = 0; vFe = vPk = vPkt = vPe = 0; vVg = 1; vKey = -1;
        f1s1 = f1s2 = f2s1 = f2s2 = 0;
        hn = 0; bend = 0; lkVel = 0.8;
        phSel = 0; phLast = -1; phS = -1000; phOn = false; phClock = 0; phRoot = 48; phVel = 0.8;
        phIval = 0; phRel = 0;
    }

    // @slider, from the 19 slider values in order
    void setParams(const double* v)
    {
        const double* s = v - 1; // s[n] = slider n
        const double sr = fSr;
        osc = std::floor(s[1] + 0.5);
        oct = std::floor(s[2] + 0.5);
        detOn = s[3] > 0;
        detR = std::pow(2.0, s[3] / 1200);
        subL = s[4] / 100;
        punch = s[5] / 100;
        drop = 3 + 21 * punch;
        drv = s[6] / 100;
        drvG = 1 + 8 * drv;
        drvN = 1 / (1 + 0.6 * drv);
        cut = s[7];
        res = s[8] / 100;
        k1 = 1 / 0.541;
        k2 = 1 / (0.707 + 9 * res * res);
        envOct = s[9] / 100 * 6;
        fdc = std::exp(-3 / (s[10] / 1000 * sr));
        aInc = 1 / (s[11] / 1000 * sr);
        dC = std::exp(-3 / (s[12] / 1000 * sr));
        sus = s[13] / 100;
        rC = std::exp(-3 / (s[14] / 1000 * sr));
        gmode = s[15] >= 0.5;
        gl = s[16];
        glC = gl > 0 ? 1 - std::exp(-3 / (gl / 1000 * sr)) : 1;
        slC = 1 - std::exp(-3 / (std::max(gl, 60.0) / 1000 * sr));
        vs = s[17] / 100;
        vol = std::pow(10.0, s[19] / 20);
        pkC = std::exp(-1 / (0.02 * sr));
        paC = 1 - std::exp(-1 / (0.0015 * sr));
        peC = std::exp(-1 / (0.035 * sr));
        if (std::fabs(s[18] - phLast) >= 0.00001) // EEL2 != is fuzzy
        {
            phLast = s[18];
            phSet(std::max(0.0, std::min(8.0, std::floor(s[18] + 0.5))));
        }
    }

    // @block: the block's MIDI (offsets within it), the transport; request = a phrase + 1 chosen
    // in the interface, 0 for none
    void beginBlock(const MidiEv* ev, uint32_t count, bool isPlaying, double tempo, double beatPos, int request)
    {
        mqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + mqN, mq);
        mqI = 0;
        playing = isPlaying;
        bInc = tempo / (60 * fSr);
        b0 = beatPos;
        si = 0;
        if (request > 0)
            phSet(request - 1);
    }

    void process(float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double o = sample();
            outL[n] = float(o);
            outR[n] = float(o);
        }
    }

    // for the display
    double phSel = 0, phS = -1000;
    bool phOn = false;
    int hn = 0;
    double HS[32] = {};
    double vTgt = 48, vEnv = 0;
    int vStage = 0;

private:
    double sample()
    {
        while (mqI < mqN && double(mq[mqI].offset) <= si)
        {
            midiEv(mq[mqI].st, mq[mqI].d1, mq[mqI].d2);
            ++mqI;
        }

        if (phSel > 0 && hn > 0)
        {
            const double bb = playing ? b0 + si * bInc : phClock;
            phClock = bb + bInc;
            const double s = std::floor(bb * 4 + 0.000001);
            if (s != phS)
            {
                if (phS > -1000 || bb * 4 - s < 0.5)
                    phStep(s);
                phS = s;
            }
            if (phOn && bb * 4 + 0.000001 >= phRel)
            {
                phOn = false;
                vRelease();
            }
        }
        si += 1;

        double out = 0;
        if (vStage > 0)
        {
            const double sr = fSr;
            vCur += (vTgt - vCur) * vGc;
            const double p = vCur + oct * 12 + bend + (osc == 3 ? drop * vPe : 0);
            const double dt = std::min(0.45, 440 * std::pow(2.0, (p - 69) / 12) / sr);
            vPh += dt; vPh -= std::floor(vPh);
            double y = wave(vPh, dt);
            if (detOn)
            {
                const double dt2 = std::min(0.45, dt * detR);
                vPh2 += dt2; vPh2 -= std::floor(vPh2);
                y = (y + wave(vPh2, dt2)) * 0.6;
            }
            vPhs += dt * 0.5; vPhs -= std::floor(vPhs);
            y += subL * std::sin(2 * kPi * vPhs);

            const double fc = std::max(20.0, std::min(0.45 * sr, cut * std::pow(2.0, envOct * vFe * vVg)));
            const double g = std::tan(kPi * fc / sr);
            double a1 = 1 / (1 + g * (g + k1)), a2 = g * a1, a3 = g * a2;
            double v3 = y - f1s2, v1 = a1 * f1s1 + a2 * v3, v2 = f1s2 + a2 * f1s1 + a3 * v3;
            f1s1 = 2 * v1 - f1s1; f1s2 = 2 * v2 - f1s2;
            a1 = 1 / (1 + g * (g + k2)); a2 = g * a1; a3 = g * a2;
            v3 = v2 - f2s2; v1 = a1 * f2s1 + a2 * v3; v2 = f2s2 + a2 * f2s1 + a3 * v3;
            f2s1 = 2 * v1 - f2s1; f2s2 = 2 * v2 - f2s2;
            y = v2;

            if (drv > 0)
            {
                const double u = std::max(-3.0, std::min(3.0, y * drvG));
                y = u * (27 + u * u) / (27 + 9 * u * u) * drvN;
            }

            vPk += (vPkt - vPk) * paC;
            out = y * vEnv * vVg * (1 + 2 * punch * vPk) * 0.42;
            vPkt *= pkC; vFe *= fdc; vPe *= peC;
            if (vStage == 1)
            {
                vEnv += aInc;
                if (vEnv >= 1)
                {
                    vEnv = 1;
                    vStage = 2;
                }
            }
            else if (vStage == 2)
            {
                vEnv = sus + (vEnv - sus) * dC;
                if (sus <= 0 && vEnv < 0.00001)
                {
                    vEnv = 0;
                    vStage = 0;
                }
            }
            else
            {
                vEnv *= rC;
                if (vEnv < 0.00001)
                {
                    vEnv = 0;
                    vStage = 0;
                }
            }
        }
        return out * vol;
    }

    double wave(double p, double dt) const
    {
        if (osc == 0)
            return std::sin(2 * kPi * p) + 0.2 * std::sin(4 * kPi * p);
        if (osc == 1)
            return 2 * p - 1 - blep(p, dt);
        if (osc == 2)
            return square(p, dt);
        return std::sin(2 * kPi * p);
    }

    void vNote(double n, double vel, bool retrig, bool glide, double gc)
    {
        if (vHas && glide)
            vGc = gc;
        else
        {
            vCur = n;
            vGc = 1;
        }
        vTgt = n;
        vHas = true;
        if (retrig || vStage == 0 || vStage == 3)
        {
            if (vEnv < 0.001)
            {
                vPh = 0; vPh2 = 0.37; vPhs = 0;
            }
            vStage = 1; vFe = 1; vPkt = 1; vPe = 1;
            vVg = 1 - vs * (1 - vel);
        }
        vGate = true;
    }

    void vRelease()
    {
        vGate = false;
        if (vStage > 0)
            vStage = 3;
    }

    void hsRemove(double n)
    {
        int i = 0;
        while (i < hn && HS[i] != n)
            ++i;
        if (i < hn)
        {
            while (i < hn - 1)
            {
                HS[i] = HS[i + 1];
                ++i;
            }
            hn -= 1;
        }
    }

    void hsPush(double n)
    {
        hsRemove(n);
        if (hn >= 32)
        {
            for (int i = 0; i < 31; ++i)
                HS[i] = HS[i + 1];
            hn = 31;
        }
        HS[hn] = n;
        hn += 1;
    }

    void phStep(double s)
    {
        const int i = int(s - std::floor(s / 16) * 16);
        const int sel = int(phSel);
        const int b = sel * 16 + i;
        const double iv = PN[b];
        if (iv == -100)
        {
            if (phOn)
            {
                phOn = false;
                vRelease();
            }
        }
        else if (iv != -200)
        {
            double t = 0;
            int j = i + 1;
            while (j < 16 && PN[sel * 16 + j] == -200)
            {
                t += 1;
                ++j;
            }
            const bool hold = j < 16 && PS[sel * 16 + j] > 0 && PN[sel * 16 + j] > -100;
            phRel = hold ? s + 1 + t + 4 : s + (1 + t) * PG[sel];
            phIval = iv;
            if (PS[b] > 0 && phOn)
                vNote(phRoot + iv, 0, false, true, slC);
            else
                vNote(phRoot + iv, phVel * (PA[b] > 0 ? 1 : 0.78), true, gmode, glC);
            phOn = true;
        }
    }

    void phSet(double k)
    {
        if (std::fabs(k - phSel) < 0.00001)
            return;
        phSel = k;
        phOn = false;
        phS = -1000;
        vRelease();
        if (hn > 0)
        {
            if (k == 0)
            {
                vKey = HS[hn - 1];
                vNote(vKey, lkVel, true, false, 1);
            }
            else
            {
                phRoot = HS[hn - 1];
                phVel = lkVel;
                phClock = 0;
            }
        }
    }

    void keyOn(double n, double v)
    {
        if (n < 36)
        {
            if (n >= 24 && n <= 32)
                phSet(n - 24);
            return;
        }
        const int was = hn;
        hsPush(n);
        lkVel = v;
        if (phSel > 0)
        {
            phRoot = n;
            phVel = v;
            if (was == 0)
            {
                phS = -1000;
                phClock = 0;
            }
            else if (phOn)
            {
                vTgt = n + phIval;
                vGc = glC;
            }
        }
        else
        {
            const bool legato = was > 0 && vGate;
            vKey = n;
            if (gmode)
                vNote(n, v, true, true, glC);
            else
                vNote(n, v, !legato, legato, glC);
        }
    }

    void keyOff(double n)
    {
        if (n < 36)
            return;
        hsRemove(n);
        if (phSel > 0)
        {
            if (hn == 0)
            {
                if (phOn)
                    vRelease();
                phOn = false;
                phS = -1000;
            }
            else
            {
                phRoot = HS[hn - 1];
                if (phOn)
                {
                    vTgt = phRoot + phIval;
                    vGc = glC;
                }
            }
        }
        else if (std::fabs(n - vKey) < 0.00001)
        {
            if (hn > 0)
            {
                vKey = HS[hn - 1];
                vNote(vKey, lkVel, false, true, glC);
            }
            else
                vRelease();
        }
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        if (t == 0x90 && d2 > 0)
            keyOn(d1, d2 / 127);
        else if (t == 0x80 || t == 0x90)
            keyOff(d1);
        else if (t == 0xB0 && (d1 == 123 || d1 == 120))
        {
            hn = 0;
            phOn = false;
            phS = -1000;
            vRelease();
        }
        else if (t == 0xE0)
            bend = (d2 * 128 + d1 - 8192) / 8192 * 2;
    }

    double fSr = 44100;
    double PN[(kNumPhrases + 1) * 16] = {}, PA[(kNumPhrases + 1) * 16] = {}, PS[(kNumPhrases + 1) * 16] = {}, PG[kNumPhrases + 1] = {};

    // @slider
    double osc = 1, oct = 0, detR = 1, subL = 0, punch = 0, drop = 0, drv = 0, drvG = 1, drvN = 1;
    bool detOn = false, gmode = false;
    double cut = 900, res = 0, k1 = 1, k2 = 1, envOct = 0, fdc = 0, aInc = 1, dC = 0, sus = 0, rC = 0;
    double gl = 0, glC = 1, slC = 1, vs = 0, vol = 1, pkC = 0, paC = 1, peC = 0;

    // the voice
    bool vGate = false, vHas = false;
    double vCur = 48, vGc = 1, vPh = 0, vPh2 = 0, vPhs = 0, vFe = 0, vPk = 0, vPkt = 0, vPe = 0, vVg = 1, vKey = -1;
    double f1s1 = 0, f1s2 = 0, f2s1 = 0, f2s2 = 0;
    double bend = 0, lkVel = 0.8;

    // the phrase player
    double phLast = -1, phClock = 0, phRoot = 48, phVel = 0.8, phIval = 0, phRel = 0;

    // the block
    MidiEv mq[kMaxEvents];
    uint32_t mqN = 0, mqI = 0;
    bool playing = false;
    double bInc = 0, b0 = 0, si = 0;
};

} // namespace nullbass
