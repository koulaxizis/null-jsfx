/*
 * NULL JSFX - Riff (VST3/CLAP): the pattern (rf_gen in riff.jsfx) and the MIDI processing (its
 * @block), step for step, so the MIDI that goes out is the same message for message, at the
 * same sample
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullriff {

// scales: size, then the semitones (the Scale slider's order)
static const int kScales[12][8] = {
    { 7, 0, 2, 4, 5, 7, 9, 11 },  // Major
    { 7, 0, 2, 3, 5, 7, 8, 10 },  // Minor
    { 7, 0, 2, 3, 5, 7, 8, 11 },  // Harmonic Minor
    { 7, 0, 2, 3, 5, 7, 9, 11 },  // Melodic Minor
    { 7, 0, 2, 3, 5, 7, 9, 10 },  // Dorian
    { 7, 0, 1, 3, 5, 7, 8, 10 },  // Phrygian
    { 7, 0, 2, 4, 6, 7, 9, 11 },  // Lydian
    { 7, 0, 2, 4, 5, 7, 9, 10 },  // Mixolydian
    { 7, 0, 1, 3, 5, 6, 8, 10 },  // Locrian
    { 5, 0, 2, 4, 7, 9, 0, 0 },   // Major Pentatonic
    { 5, 0, 3, 5, 7, 10, 0, 0 },  // Minor Pentatonic
    { 6, 0, 3, 5, 6, 7, 10, 0 },  // Blues
};

static constexpr int kMaxSteps = 32;

struct Params {
    int key = 0, scale = 0, seed = 1, steps = 16, rate = 1, octs = 1, lock = 0;
    double dens = 0.75, nlen = 0.8, vrng = 0.3, acc = 0.25, vari = 0.5;
};

// the step length in beats
static double stepBeats(int rate)
{
    return rate == 0 ? 0.5 : rate == 1 ? 0.25 : rate == 2 ? 0.125 : rate == 3 ? 1.0 / 3.0 : rate == 4 ? 1.0 / 6.0 : 1.0 / 12.0;
}

// semitones of degree j (0 = the tonic) of scale s, over the octaves
static int semi(int s, int j)
{
    const int m = kScales[s][0];
    const int o = j / m;
    return kScales[s][1 + j - o * m] + 12 * o;
}

// the pattern, always 32 steps
struct Pattern {
    bool on[kMaxSteps] = {};
    int deg[kMaxSteps] = {};
    double vr[kMaxSteps] = {};
    bool acc[kMaxSteps] = {};
    int top = 7; // the highest degree (the range's top root)
};

// one Park-Miller step in exact doubles, a value in [0, 1)
static double pm(double& x)
{
    x *= 16807.0;
    x -= std::floor(x / 2147483647.0) * 2147483647.0;
    return x / 2147483648.0;
}

static void generate(const Params& p, Pattern& o)
{
    double rs = double(p.lock > 0 ? p.lock : p.seed) * 7919.0 + 13.0;
    double ps = double(p.seed) * 104729.0 + 71.0;
    for (int k = 0; k < 4; ++k)
    {
        pm(rs);
        pm(ps);
    }
    const int sz = kScales[p.scale][0];
    const int top = sz * p.octs;
    o.top = top;
    const int m = p.rate >= 3 ? 3 : 4;
    int prev = 0;
    for (int i = 0; i < kMaxSteps; ++i)
    {
        const double g = pm(rs), v = pm(rs), a = pm(rs);
        o.on[i] = i == 0 || g < p.dens;
        o.vr[i] = v;
        o.acc[i] = a < p.acc;
        const double p1 = pm(ps), p2 = pm(ps), p3 = pm(ps), p4 = pm(ps);
        int d;
        if (i == 0)
            d = 0;
        else if (i >= m && p1 >= p.vari)
            d = o.deg[i - m];
        else if (p2 < 0.3)
            d = sz * int(std::floor(p3 * double(p.octs + 1)));
        else if (p2 < 0.7)
        {
            d = prev + (p3 < 0.5 ? -1 : 1) * (1 + int(std::floor(p4 * 2.0)));
            if (d < 0)
                d = -d;
            if (d > top)
                d = 2 * top - d;
            d = std::max(0, std::min(top, d));
        }
        else
            d = int(std::floor(p3 * double(top + 1)));
        o.deg[i] = std::min(top, d);
        prev = o.deg[i];
    }
}

// the note degree g above the root plays when key n leads (the root: the scale note at or below n)
static int noteFor(int n, int g, const Params& p)
{
    const int s = p.scale;
    const int pc = (n - p.key + 120) % 12;
    int d = kScales[s][0] - 1;
    while (d > 0 && kScales[s][1 + d] > pc)
        --d;
    return n - pc + semi(s, d + g);
}

// ---- the MIDI processing ----------------------------------------------------------------------

class RiffProcessor
{
public:
    static constexpr int kKeysMax = 128;
    static constexpr int kOutMax = 8192;

    void setSampleRate(double sr) { fSr = sr; }

    // @init
    void reset()
    {
        fStn = 0;
        fRun = false;
        fCurNote = -1;
        fCurCh = 0;
        fCurOff = 0.0;
        fCurStep = -1;
        fPos = 0.0;
        fWasPlay = false;
        fNk = 0.0;
        fNst = 0.0;
        fLastK = -1000000000.0;
        fOutN = 0;
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        generate(fP, fPat);
    }

    // the start of @block: the host's tempo, whether it plays and its beat position
    void begin(double tempo, bool playing, double beat)
    {
        fOutN = 0;
        fSpb = fSr * 60.0 / std::max(tempo, 1.0);
        fSb = stepBeats(fP.rate);
        fSl = fSb * fSpb;
        fPlay = playing;
        if (!fPlay && fWasPlay)
            stop(fPos);
        if (fPlay && !fWasPlay)
            fLastK = -1000000000.0;
        fWasPlay = fPlay;
        if (fPlay)
        {
            fB0 = beat;
            fNk = std::ceil(fB0 / fSb - 0.000001);
            if (fNk == fLastK)
                fNk += 1.0;
            fNst = fPos + (fNk * fSb - fB0) * fSpb;
        }
    }

    void input(uint32_t ofs, int a, int b, int c)
    {
        const double t = fPos + ofs;
        advance(t);
        in(t, a, b, c);
    }

    // the rest of the block, then what goes out: send(offset, status, d1, d2) in time order
    template <class Send>
    void end(uint32_t frames, Send&& send)
    {
        advance(fPos + frames);
        for (int i = 0; i < fOutN; ++i)
            send(fOut[i].ofs, fOut[i].a, fOut[i].b, fOut[i].c);
        fOutN = 0;
        fPos += frames;
    }

    const Pattern& pattern() const { return fPat; }
    int step() const { return fRun ? fCurStep : -1; }
    int root() const { return fRun && fStn > 0 ? fStk[fStn - 1] & 127 : -1; }
    int sounding() const { return fCurNote; }

private:
    struct Out { uint32_t ofs; int a, b, c; };

    void send(double t, int a, int b, int c)
    {
        if (fOutN < kOutMax)
            fOut[fOutN++] = Out { uint32_t(t - fPos), a, b, c };
    }

    int vel(int v0, int i) const
    {
        double x = double(v0) * (1.0 - fP.vrng * fPat.vr[i]);
        if (fPat.acc[i])
            x += (127.0 - x) * 0.6;
        return int(std::max(1.0, std::min(127.0, std::floor(x + 0.5))));
    }

    void off(double t)
    {
        if (fCurNote >= 0)
        {
            send(t, 0x80 | fCurCh, fCurNote, 0);
            fCurNote = -1;
        }
    }

    void stop(double t)
    {
        off(t);
        fStn = 0;
        fRun = false;
        fCurStep = -1;
    }

    void stepAt(double t, double ex)
    {
        const int steps = fP.steps;
        const int i = int(fNk - double(steps) * std::floor(fNk / double(steps)));
        if (fRun)
        {
            fCurStep = i;
            off(t);
            if (fPat.on[i])
            {
                const int s = fStk[fStn - 1];
                const int n = noteFor(s & 127, fPat.deg[i], fP);
                if (n >= 0 && n <= 127)
                {
                    fCurCh = s / 128;
                    fCurNote = n;
                    send(t, 0x90 | fCurCh, n, vel(fStv[fStn - 1], i));
                    fCurOff = std::max(t + 1.0, std::floor(ex + fSl * fP.nlen + 0.001));
                }
            }
        }
        fLastK = fNk;
        fNk += 1.0;
        fNst = fPlay ? fPos + (fNk * fSb - fB0) * fSpb : fNst + fSl;
    }

    void advance(double tend)
    {
        const double never = 1152921504606846976.0; // 2^60
        // a step lands on the sample its exact time falls in (0.001 more, as in the JSFX)
        for (;;)
        {
            const double to = fCurNote >= 0 ? fCurOff : never;
            const double ts = (fPlay || fRun) ? std::max(fPos, std::floor(fNst + 0.001)) : never;
            if (to <= ts && to < tend)
                off(to);
            else if (ts < tend)
                stepAt(ts, fNst);
            else
                break;
        }
    }

    void del(int s)
    {
        int f = -1;
        for (int i = 0; f < 0 && i < fStn; ++i)
            if (fStk[i] == s)
                f = i;
        if (f < 0)
            return;
        for (int i = f; i + 1 < fStn; ++i)
        {
            fStk[i] = fStk[i + 1];
            fStv[i] = fStv[i + 1];
        }
        --fStn;
    }

    void in(double t, int a, int b, int c)
    {
        const int st = a & 0xF0, ch = a & 0x0F, s = ch * 128 + b;
        if (st == 0x90 && c > 0)
        {
            del(s);
            if (fStn < kKeysMax)
            {
                fStk[fStn] = s;
                fStv[fStn] = c;
                ++fStn;
            }
            if (!fRun)
            {
                fRun = true;
                if (!fPlay)
                {
                    fNk = 0.0;
                    fNst = t;
                }
            }
        }
        else if (st == 0x80 || st == 0x90)
        {
            del(s);
            if (fStn == 0 && fRun)
            {
                off(t);
                fRun = false;
                fCurStep = -1;
            }
        }
        else if (st == 0xA0)
        {
            if (fStn > 0 && fCurNote >= 0 && fStk[fStn - 1] == s)
                send(t, 0xA0 | fCurCh, fCurNote, c);
        }
        else if (st == 0xB0 && (b == 120 || b == 123))
        {
            for (int i = 0; i < fStn;)
            {
                if (fStk[i] / 128 == ch)
                    del(fStk[i]);
                else
                    ++i;
            }
            if (fCurNote >= 0 && fCurCh == ch)
                off(t);
            if (fStn == 0 && fRun)
            {
                off(t);
                fRun = false;
                fCurStep = -1;
            }
            send(t, a, b, c);
        }
        else
            send(t, a, b, c);
    }

    Params fP;
    Pattern fPat;
    double fSr = 44100.0;
    int fStk[kKeysMax] = {}, fStv[kKeysMax] = {};
    int fStn = 0;
    bool fRun = false;
    int fCurNote = -1, fCurCh = 0, fCurStep = -1;
    double fCurOff = 0.0;
    double fPos = 0.0;
    bool fWasPlay = false, fPlay = false;
    double fNk = 0.0, fNst = 0.0, fLastK = -1000000000.0;
    double fSpb = 0.0, fSb = 0.25, fSl = 0.0, fB0 = 0.0;
    Out fOut[kOutMax];
    int fOutN = 0;
};

} // namespace nullriff
