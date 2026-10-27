/*
 * NULL JSFX - MIDI LFO (VST3/CLAP): the lf_* functions and @block of midi_lfo.jsfx, in the
 * same order, so every message goes out at the same sample with the same bytes
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullmidilfo {

struct Params
{
    int shape = 0, sync = 1, dest = 0, cc = 1, channel = 1, retrigger = 0, seed = 1;
    double rate = 1.0, length = 1.0, phase = 0.0, min = 0.0, max = 1.0;
};

class MidiLfoProcessor
{
public:
    static constexpr int kTick = 64;
    static constexpr int kMaxEvents = 1024;

    void setSampleRate(double sr) { fSr = sr; }

    // @init
    void reset()
    {
        fPos = fCurT = 0.0;
        fPh = 0.0;
        fCyc = -1.0;
        fLast = -1.0;
        fSeedWas = -1;
        fDestWas = -1;
        fPrev = fCur = 0.0;
        fOutU = 0.5;
        fOutP = 0.0;
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        if (p.seed != fSeedWas)
        {
            fRs = double(p.seed) * 7919.0 + 13.0;
            fCur = rnd();
            next();
            fSeedWas = p.seed;
        }
        const int key = p.dest * 4096 + p.cc * 16 + (p.channel - 1);
        if (key != fDestWas)
        {
            fLast = -1.0;
            fDestWas = key;
        }
    }

    // the start of a block: the host's transport (beat at the block's first sample)
    void begin(uint32_t frames, bool playing, double tempo, double beat)
    {
        fFrames = frames;
        fPlaying = playing;
        fTempo = tempo;
        fBeat = beat;
        fN = 0;
    }

    void input(uint32_t ofs, int a, int b, int c)
    {
        if (fN < kMaxEvents)
            fEv[fN++] = Ev { ofs, a, b, c };
    }

    // the block's messages in time order: the incoming ones and the LFO's, send(ofs, a, b, c)
    template <class Send>
    void end(uint32_t frames, Send&& send)
    {
        const double end = fPos + frames;
        double tn = std::ceil(fPos / kTick) * kTick;
        int i = 0;
        while (i < fN || tn < end)
        {
            if (i < fN && (tn >= end || fPos + fEv[i].ofs <= tn))
            {
                const Ev& e = fEv[i];
                if (fP.retrigger && (e.a & 0xF0) == 0x90 && e.c > 0 && !(fP.sync && fPlaying))
                {
                    phase(fPos + e.ofs);
                    fPh = 0.0;
                }
                send(e.ofs, e.a, e.b, e.c);
                ++i;
            }
            else
            {
                tick(tn, uint32_t(tn - fPos), send);
                tn += kTick;
            }
        }
        fPos = end;
    }

    double outPhase() const { return fOutP; }
    double outValue() const { return fOutU; }
    double randomPrev() const { return fPrev; }
    double randomNext() const { return fCur; }

    // lf_wave: the wave at phase p (0..1), -1..1, for the display too
    static double wave(int shape, double p, double prev, double cur)
    {
        switch (shape)
        {
        case 0: return std::sin(2.0 * M_PI * p);
        case 1: return p < 0.25 ? 4.0 * p : p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0;
        case 2: return 2.0 * p - 1.0;
        case 3: return 1.0 - 2.0 * p;
        case 4: return p < 0.5 ? 1.0 : -1.0;
        case 5: return cur;
        default: return prev + (cur - prev) * (1.0 - std::cos(M_PI * p)) / 2.0;
        }
    }

private:
    struct Ev { uint32_t ofs; int a, b, c; };

    // lf_rnd: one Park-Miller step in exact doubles, a value in [-1, 1)
    double rnd()
    {
        fRs *= 16807.0;
        fRs -= std::floor(fRs / 2147483647.0) * 2147483647.0;
        return fRs / 2147483648.0 * 2.0 - 1.0;
    }

    void next()
    {
        fPrev = fCur;
        fCur = rnd();
    }

    // lf_phase
    double phase(double t)
    {
        if (fP.sync && fPlaying)
        {
            const double x = (fBeat + (t - fPos) * fTempo / (60.0 * fSr)) / fP.length;
            const double c = std::floor(x);
            fPh = x - c;
            if (c != fCyc)
            {
                fCyc = c;
                next();
            }
        }
        else
        {
            fPh += (t - fCurT) * (fP.sync ? fTempo / (60.0 * fSr * fP.length) : fP.rate / fSr);
            if (fPh >= 1.0)
            {
                fPh -= std::floor(fPh);
                fCyc += 1.0;
                next();
            }
        }
        fCurT = t;
        const double x = fPh + fP.phase;
        return x - std::floor(x);
    }

    // lf_tick
    template <class Send>
    void tick(double t, uint32_t ofs, Send&& send)
    {
        const double p = phase(t);
        fOutP = p;
        fOutU = (wave(fP.shape, p, fPrev, fCur) + 1.0) / 2.0;
        const double v = fP.min + (fP.max - fP.min) * fOutU;
        const int ch = fP.channel - 1;
        double o;
        if (fP.dest == 1)
        {
            o = std::floor(v * 16383.0 + 0.5);
            if (o != fLast)
                send(ofs, 0xE0 | ch, int(o) & 127, int(std::floor(o / 128.0)));
        }
        else
        {
            o = std::floor(v * 127.0 + 0.5);
            if (o != fLast)
            {
                if (fP.dest == 0)
                    send(ofs, 0xB0 | ch, fP.cc, int(o));
                else
                    send(ofs, 0xD0 | ch, int(o), 0);
            }
        }
        fLast = o;
    }

    Params fP;
    double fSr = 48000.0;
    double fPos = 0.0, fCurT = 0.0, fPh = 0.0, fCyc = -1.0, fLast = -1.0, fRs = 0.0;
    double fPrev = 0.0, fCur = 0.0, fOutU = 0.5, fOutP = 0.0;
    double fTempo = 120.0, fBeat = 0.0;
    bool fPlaying = false;
    uint32_t fFrames = 0;
    int fSeedWas = -1, fDestWas = -1, fN = 0;
    Ev fEv[kMaxEvents] = {};
};

} // namespace nullmidilfo
