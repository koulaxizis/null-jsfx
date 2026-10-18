/*
 * NULL JSFX - Arpeggiator (VST3/CLAP): the MIDI processing of arpeggiator.jsfx, framework-free,
 * the same steps as its ar_* functions so the MIDI that goes out is the same message for message,
 * at the same sample
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullarp {

static constexpr int kLaneSteps = 16;
static constexpr int kMaxNotes = 64;

struct Params {
    int rate = 6, pattern = 0, octaves = 1, latch = 0;
    double gate = 50.0, swing = 0.0, steps = 16.0, seed = 1.0;
    double lane[kLaneSteps] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
};

// the step in beats for each Rate (1/4, 1/4D, 1/4T, 1/8 ... 1/32)
static double stepBeats(int rate)
{
    return rate == 0 ? 1.0 : rate == 1 ? 1.5 : rate == 2 ? 2.0 / 3.0 : rate == 3 ? 0.5 : rate == 4 ? 0.75
         : rate == 5 ? 1.0 / 3.0 : rate == 6 ? 0.25 : rate == 7 ? 0.375 : rate == 8 ? 1.0 / 6.0 : 0.125;
}

class ArpProcessor
{
public:
    static constexpr int kKeys = 16 * 128;
    static constexpr int kQueue = 2048;

    struct Msg { double t; int a, b, c; };

    void setSampleRate(double sr) { fSr = sr; }

    // @init
    void reset()
    {
        std::memset(fAct, 0, sizeof(fAct));
        std::memset(fCnt, 0, sizeof(fCnt));
        fQn = 0;
        fPos = 0.0;
        fFb = 0.0;
        fNl = 0;
        fPhys = 0;
        fRun = false;
        fNextK = 0.0;
        fAi = 0;
        fWasPlay = false;
        fPm = 1.0;
        fPmSeed = -1.0;
        fCurStep = -1.0;
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        fSl = stepBeats(p.rate);
        fSwsh = p.swing / 100.0 * fSl / 3.0;
    }

    // @block, before the messages
    void begin(uint32_t frames, bool playing, double tempo, double beat)
    {
        fFrames = frames;
        fPlaying = playing;
        fBps = std::fmax(1.0, tempo) / (60.0 * fSr);
        if ((playing && !fWasPlay) || fP.seed != fPmSeed)
        {
            fPmSeed = fP.seed;
            fPm = 12345.0 + fP.seed * 7919.0;
        }
        if (fWasPlay && !playing)
            cut(fPos, -1);
        fB0 = playing ? beat : fFb;
        if (!fP.latch)
        {
            int i = 0;
            while (i < fNl)
            {
                if (fAct[fLc[i] * 128 + fLn[i]])
                    ++i;
                else
                    remove(i);
            }
            if (fNl == 0 && fRun)
            {
                fRun = false;
                endNotes(fPos, -1);
            }
        }
        if (fRun)
        {
            const double kb = first(fB0);
            if ((playing && !fWasPlay) || fNextK < kb - 1.0 || fNextK > kb + 1.0)
                fNextK = kb;
        }
        fWasPlay = playing;
    }

    // one incoming message (in time order)
    void input(uint32_t ofs, int a, int b, int c)
    {
        steps(double(ofs));
        const int st = a & 0xF0;
        if (st == 0x90 && c > 0)
            noteOn(ofs, a & 0x0F, b, c);
        else if (st == 0x80 || st == 0x90)
            noteOff(ofs, a & 0x0F, b);
        else if (st == 0xB0 && (b == 120 || b == 123))
        {
            cut(fPos + ofs, a & 0x0F);
            put(fPos + ofs, a, b, c);
        }
        else
            put(fPos + ofs, a, b, c);
    }

    // the rest of @block: the steps left, then the messages due: send(offset, status, d1, d2)
    template <class Send>
    void end(uint32_t frames, Send&& send)
    {
        steps(double(frames));
        if (!fRun)
            fCurStep = -1.0;
        const double bEnd = fPos + frames;
        int i = 0;
        for (; i < fQn && fQ[i].t < bEnd; ++i)
        {
            const Msg& m = fQ[i];
            const uint32_t ofs = uint32_t(m.t - fPos);
            const int st = m.a & 0xF0, k = (m.a & 0x0F) * 128 + m.b;
            if (st == 0x90)
            {
                if (fCnt[k] > 0)
                    send(ofs, 0x80 | (m.a & 0x0F), m.b, 0);
                send(ofs, m.a, m.b, m.c);
                fCnt[k] += 1;
            }
            else if (st == 0x80)
            {
                if (fCnt[k] > 0)
                {
                    fCnt[k] -= 1;
                    if (fCnt[k] == 0)
                        send(ofs, m.a, m.b, m.c);
                }
            }
            else
                send(ofs, m.a, m.b, m.c);
        }
        if (i > 0)
        {
            for (int j = 0; j + i < fQn; ++j)
                fQ[j] = fQ[j + i];
            fQn -= i;
        }
        fFb = fB0 + double(frames) * fBps;
        fPos = bEnd;
    }

    // for the display
    bool sounding(int k) const { return fCnt[k] > 0; }
    int noteCount() const { return fNl; }
    int note(int i) const { return fLn[i]; }
    int currentStep() const { return int(fCurStep); }
    bool playing() const { return fPlaying; }

private:
    void put(double t, int a, int b, int c)
    {
        if (fQn >= kQueue)
            return;
        int i = fQn;
        while (i > 0 && fQ[i - 1].t > t)
        {
            fQ[i] = fQ[i - 1];
            --i;
        }
        fQ[i] = Msg { t, a, b, c };
        ++fQn;
    }

    // the arp's note-offs still waiting (on channel ch, -1 = every channel) move to time t
    void endNotes(double t, int ch)
    {
        for (int i = 0; i < fQn; ++i)
        {
            const int a = fQ[i].a;
            if ((a & 0xF0) == 0x80 && fQ[i].t > t && (ch < 0 || (a & 0x0F) == ch))
            {
                Msg x = fQ[i];
                x.t = t;
                int j = i;
                while (j > 0 && fQ[j - 1].t > t)
                {
                    fQ[j] = fQ[j - 1];
                    --j;
                }
                fQ[j] = x;
            }
        }
    }

    int find(int ch, int n) const
    {
        for (int i = 0; i < fNl; ++i)
            if (fLn[i] == n && fLc[i] == ch)
                return i;
        return -1;
    }

    void remove(int i)
    {
        for (int j = i; j + 1 < fNl; ++j)
        {
            fLn[j] = fLn[j + 1];
            fLc[j] = fLc[j + 1];
            fLv[j] = fLv[j + 1];
        }
        --fNl;
    }

    void cut(double t, int ch)
    {
        endNotes(t, ch);
        int i = 0;
        while (i < fNl)
        {
            if (ch < 0 || fLc[i] == ch)
                remove(i);
            else
                ++i;
        }
        const int k0 = ch < 0 ? 0 : ch * 128, cnt = ch < 0 ? kKeys : 128;
        for (int k = k0; k < k0 + cnt; ++k)
            if (fAct[k])
            {
                fAct[k] = 0;
                --fPhys;
            }
        if (fNl == 0)
            fRun = false;
    }

    double pmRand()
    {
        fPm *= 16807.0;
        fPm -= std::floor(fPm / 2147483647.0) * 2147483647.0;
        return fPm / 2147483647.0;
    }

    double grid(double k) const { return k * fSl + (k - 2.0 * std::floor(k / 2.0)) * fSwsh; }

    double first(double b) const
    {
        const double k = std::floor((b - 0.000001) / fSl);
        return grid(k) >= b - 0.000001 ? k : k + 1.0;
    }

    double offset(double k) const { return std::fmax(0.0, std::ceil((grid(k) - fB0) / fBps - 0.000001)); }

    void build()
    {
        for (int i = 0; i < fNl; ++i)
        {
            const int n = fLn[i], c = fLc[i];
            const double v = fLv[i];
            int j = i;
            if (fP.pattern != 4)
            {
                const int key = n * 16 + c;
                while (j > 0 && fSqn[j - 1] * 16 + fSqc[j - 1] > key)
                {
                    fSqn[j] = fSqn[j - 1];
                    fSqc[j] = fSqc[j - 1];
                    fSqv[j] = fSqv[j - 1];
                    --j;
                }
            }
            fSqn[j] = n;
            fSqc[j] = c;
            fSqv[j] = v;
        }
        fSq = fNl;
        for (int o = 1; o < fP.octaves; ++o)
            for (int i = 0; i < fNl; ++i)
                if (fSqn[i] + 12 * o <= 127)
                {
                    fSqn[fSq] = fSqn[i] + 12 * o;
                    fSqc[fSq] = fSqc[i];
                    fSqv[fSq] = fSqv[i];
                    ++fSq;
                }
    }

    void note(double t, int ch, int n, double v, double lw, double gl)
    {
        const double vo = std::fmax(1.0, std::fmin(127.0, std::floor(v * lw / 100.0 + 0.5)));
        if (fQn + 2 <= kQueue)
        {
            put(t, 0x90 | ch, n, int(vo));
            put(t + gl, 0x80 | ch, n, 0);
        }
    }

    void step(double t, double k)
    {
        build();
        const double steps = fP.steps;
        const double lw = fP.lane[int(k - steps * std::floor(k / steps))];
        const double gl = std::fmax(1.0, std::floor(fP.gate / 100.0 * (grid(k + 1.0) - grid(k)) / fBps + 0.5));
        const int n = fSq;
        if (n > 0)
        {
            if (fP.pattern == 6)
            {
                if (lw > 0.0)
                    for (int i = 0; i < n; ++i)
                    {
                        bool d = false;
                        for (int j = 0; j < i; ++j)
                            if (fSqn[j] == fSqn[i] && fSqc[j] == fSqc[i])
                                d = true;
                        if (!d)
                            note(t, fSqc[i], fSqn[i], fSqv[i], lw, gl);
                    }
            }
            else
            {
                int j;
                if (fP.pattern == 5)
                    j = int(std::floor(pmRand() * n));
                else
                {
                    const int m = n > 1 && (fP.pattern == 2 || fP.pattern == 3) ? 2 * n - 2 : n;
                    j = int(fAi % m);
                    if (fP.pattern == 1)
                        j = n - 1 - j;
                    else if (fP.pattern == 2)
                    {
                        if (j >= n)
                            j = 2 * n - 2 - j;
                    }
                    else if (fP.pattern == 3)
                        j = j < n ? n - 1 - j : j - n + 1;
                }
                if (lw > 0.0)
                    note(t, fSqc[j], fSqn[j], fSqv[j], lw, gl);
            }
        }
        ++fAi;
        fCurStep = k - steps * std::floor(k / steps);
    }

    void steps(double lim)
    {
        double so = fRun ? offset(fNextK) : lim;
        while (fRun && so < lim)
        {
            step(fPos + so, fNextK);
            fNextK += 1.0;
            so = offset(fNextK);
        }
    }

    void start(uint32_t ofs)
    {
        fRun = true;
        fAi = 0;
        if (fPlaying)
            fNextK = first(fB0 + double(ofs) * fBps);
        else
        {
            fB0 = -double(ofs) * fBps;
            fNextK = 0.0;
        }
    }

    void noteOn(uint32_t ofs, int ch, int n, int v)
    {
        const int s = ch * 128 + n;
        if (fP.latch && fPhys == 0 && fNl > 0)
        {
            fNl = 0;
            fAi = 0;
        }
        if (!fAct[s])
        {
            fAct[s] = 1;
            ++fPhys;
        }
        const int f = find(ch, n);
        if (f >= 0)
            fLv[f] = v;
        else if (fNl < kMaxNotes)
        {
            fLn[fNl] = n;
            fLc[fNl] = ch;
            fLv[fNl] = v;
            ++fNl;
        }
        if (!fRun && fNl > 0)
            start(ofs);
    }

    void noteOff(uint32_t ofs, int ch, int n)
    {
        const int s = ch * 128 + n;
        if (!fAct[s])
            return;
        fAct[s] = 0;
        --fPhys;
        if (!fP.latch)
        {
            const int i = find(ch, n);
            if (i >= 0)
                remove(i);
            if (fNl == 0 && fRun)
            {
                fRun = false;
                endNotes(fPos + ofs, -1);
            }
        }
    }

    Params fP;
    double fSr = 44100.0, fSl = 0.25, fSwsh = 0.0, fBps = 0.0, fB0 = 0.0, fFb = 0.0, fPos = 0.0;
    double fNextK = 0.0, fPm = 1.0, fPmSeed = -1.0, fCurStep = -1.0;
    int64_t fAi = 0;
    uint32_t fFrames = 0;
    bool fRun = false, fWasPlay = false, fPlaying = false;
    uint8_t fAct[kKeys] = {};
    int fCnt[kKeys] = {};
    int fPhys = 0, fNl = 0, fSq = 0;
    int fLn[kMaxNotes] = {}, fLc[kMaxNotes] = {};
    double fLv[kMaxNotes] = {};
    int fSqn[kMaxNotes * 4] = {}, fSqc[kMaxNotes * 4] = {};
    double fSqv[kMaxNotes * 4] = {};
    Msg fQ[kQueue] = {};
    int fQn = 0;
};

} // namespace nullarp
