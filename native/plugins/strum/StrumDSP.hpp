/*
 * NULL JSFX - Strum, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/strum.jsfx, computed in double in the EEL2 order. The keys held
 * make the chord; the first key opens a Collect window, then the chord is strummed string by
 * string, Speed apart (st_strum): lowest first down, highest first up, softer per string by
 * Velocity Spread, up strokes at Upstroke Velocity, Humanize by an exact Park-Miller step. Rhythm
 * Once strums once (a key added later strums again); the rhythms strum again on the tempo while
 * keys are held, each string lasting Gate of the step. A key's note-off ends its note and drops
 * its waiting strings (ids and keys, as the JSFX). Per sample: the MIDI that came in, the strum
 * that is due, then the strings that are due; send(ofs, a, b, c) gets them in that order. The
 * queue and the tables are members, so nothing is allocated while it plays.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullstrum {

struct Params {
    int direction = 0, rhythm = 0;
    double speed = 12.0, gate = 85.0, spread = 25.0, upstroke = 80.0, humanize = 0.0, collect = 25.0;
};

class StrumProcessor {
public:
    static constexpr int kQueue = 256;
    static constexpr int kEMax = 4096;

    void setSampleRate(double sr) noexcept { fSr = sr > 0.0 ? sr : 48000.0; }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fGate = p.gate / 100.0;
        fSpread = p.spread / 100.0;
        fUpv = p.upstroke / 100.0;
    }

    // @init
    void reset() noexcept
    {
        fQn = fQi = 0;
        fEh = fEt = 0;
        std::memset(fSnd, 0, sizeof(fSnd));
        std::memset(fHeld, 0, sizeof(fHeld));
        fNheld = 0.0;
        fNextId = 1.0;
        fNow = 0.0;
        fStrumAt = -1.0;
        fRunning = false;
        fPat = 0.0;
        fAlt = 0;
        fStrums = 0;
        fSeed = 12345.0;
    }

    // @block: the tempo, then the messages that came in (offsets clamped to the block)
    void begin(uint32_t frames, double tempo) noexcept
    {
        fQn = fQi = 0;
        fFrames = frames;
        fSpd = std::floor(fP.speed * 0.001 * fSr);
        fHum = std::floor(fP.humanize * 0.001 * fSr);
        fCol = std::floor(fP.collect * 0.001 * fSr);
        const int r = fP.rhythm;
        fStep = std::fmax(1.0, std::floor(60.0 / tempo * (r == 1 ? 1.0 : r == 3 ? 0.25 : 0.5) * fSr));
        fLen = std::fmax(1.0, std::floor(fStep * fGate));
        fSi = 0;
    }
    void input(uint32_t ofs, int a, int b, int c) noexcept
    {
        if (fQn >= kQueue)
            return;
        Msg& m = fQ[fQn++];
        m.ofs = fFrames > 0 ? (ofs < fFrames ? ofs : fFrames - 1) : 0;
        m.a = a;
        m.b = b;
        m.c = c;
    }

    int strums() const noexcept { return fStrums; }
    int step() const noexcept { return int(fPat); }
    int held() const noexcept { return int(fNheld); }
    bool running() const noexcept { return fP.rhythm > 0 && fRunning && fStrumAt >= 0.0; }

    // the stroke of rhythm r at step i: 0 rest, 1 down, 2 up, 3 by Direction (st_pat)
    static int pattern(int r, int i) noexcept
    {
        const int k = i % 8;
        if (r == 4)
            return k % 2 == 0 ? 1 : 2;
        if (r == 5)
            return k == 0 || k == 2 || k == 6 ? 1 : (k == 1 || k == 4 ? 0 : 2);
        if (r == 6)
            return k % 2 == 0 ? 0 : 1;
        return 3;
    }

    // one sample of @sample (the audio passes through untouched)
    template <class Send>
    void processSample(Send&& send) noexcept
    {
        const uint32_t si = fSi;
        while (fQi < fQn && fQ[fQi].ofs <= si)
        {
            const Msg& m = fQ[fQi];
            const int st = m.a & 0xF0;
            const int ch = m.a & 15;
            if (st == 0x90 && m.c > 0)
            {
                release(si, ch * 128 + m.b, send);
                fHeld[ch * 128 + m.b] = m.c;
                fNheld += 1.0;
                if (fP.rhythm == 0)
                {
                    if (fStrumAt < 0.0)
                        fStrumAt = fNow + fCol;
                }
                else if (!fRunning || fStrumAt < 0.0)
                {
                    fRunning = true;
                    fPat = 0.0;
                    fStrumAt = fNow + fCol;
                }
            }
            else if (st == 0x80 || st == 0x90)
                release(si, ch * 128 + m.b, send);
            else
            {
                if (st == 0xB0 && (m.b == 120 || m.b == 123))
                    flush(si, ch, send);
                send(si, m.a, m.b, m.c);
            }
            ++fQi;
        }
        if (fStrumAt >= 0.0 && fStrumAt <= fNow)
        {
            if (fP.rhythm == 0)
            {
                strum(fNow, 3);
                fStrumAt = -1.0;
                fRunning = false;
            }
            else
            {
                const int d = pattern(fP.rhythm, int(fPat));
                if (d > 0)
                    strum(fNow, d);
                fPat += 1.0;
                fStrumAt += fStep;
            }
        }
        while (fEh < fEt && fE[fEh].t <= fNow)
        {
            const Ev& e = fE[fEh];
            if (e.st >= 0x90)
                noteOn(si, e.st & 15, e.nt, e.v, e.id, send);
            else
                noteOff(si, e.st & 15, e.nt, e.id, send);
            ++fEh;
        }
        if (fEh >= fEt)
            fEh = fEt = 0;
        fNow += 1.0;
        ++fSi;
    }

private:
    struct Msg { uint32_t ofs; int a, b, c; };
    struct Ev { double t; int st, nt, v; double id; int s; };

    double rand() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    template <class Send>
    void noteOn(uint32_t ofs, int ch, int nt, int v, double id, Send& send) noexcept
    {
        const int k = ch * 128 + nt;
        if (fSnd[k] != 0.0)
            send(ofs, 0x80 | ch, nt, 0);
        send(ofs, 0x90 | ch, nt, v);
        fSnd[k] = id;
    }

    template <class Send>
    void noteOff(uint32_t ofs, int ch, int nt, double id, Send& send) noexcept
    {
        const int k = ch * 128 + nt;
        if (fSnd[k] == id)
        {
            send(ofs, 0x80 | ch, nt, 0);
            fSnd[k] = 0.0;
        }
    }

    // st_push: a waiting note, kept in time order (after the ones at the same time)
    void push(double t, int st, int nt, int v, double id, int s) noexcept
    {
        if (fEt >= kEMax)
        {
            for (int i = 0; i < fEt - fEh; ++i)
                fE[i] = fE[fEh + i];
            fEt -= fEh;
            fEh = 0;
        }
        int i = fEt;
        while (i > fEh && fE[i - 1].t > t)
        {
            fE[i] = fE[i - 1];
            --i;
        }
        fE[i] = { t, st, nt, v, id, s };
        ++fEt;
    }

    // st_drop: the waiting notes of key s (s < 0: of channel -1 - s)
    void drop(int s) noexcept
    {
        int j = fEh;
        for (int i = fEh; i < fEt; ++i)
            if (s >= 0 ? fE[i].s != s : (fE[i].st & 15) != -1 - s)
            {
                if (j != i)
                    fE[j] = fE[i];
                ++j;
            }
        fEt = j;
    }

    // st_strum: a strum of the held chord at time t
    void strum(double t, int d) noexcept
    {
        if (d == 3)
        {
            const int m = fP.direction;
            if (m == 0)
                d = 1;
            else if (m == 1)
                d = 2;
            else if (m == 2)
            {
                d = fAlt ? 2 : 1;
                fAlt = 1 - fAlt;
            }
            else
                d = rand() < 0.5 ? 1 : 2;
        }
        int n = 0;
        for (int nt = 0; nt < 128; ++nt)
            for (int ch = 0; ch < 16; ++ch)
                if (fHeld[ch * 128 + nt] > 0)
                    fLst[n++] = ch * 128 + nt;
        for (int k = 0; k < n; ++k)
        {
            const int s = fLst[d == 1 ? k : n - 1 - k];
            const int ch = s / 128, nt = s - ch * 128;
            double v = std::floor(double(fHeld[s]) * (1.0 - fSpread * double(k) / std::fmax(1.0, double(n - 1))) *
                                  (d == 2 ? fUpv : 1.0) + 0.5);
            v = std::fmax(1.0, std::fmin(127.0, v));
            const double j = fHum > 0.0 ? std::floor(rand() * fHum) : 0.0;
            if (fEt - fEh + 2 <= kEMax)
            {
                push(t + k * fSpd + j, 0x90 | ch, nt, int(v), fNextId, s);
                if (fP.rhythm > 0)
                    push(t + k * fSpd + j + fLen, 0x80 | ch, nt, 0, fNextId, s);
                fNextId += 1.0;
            }
        }
        if (n > 0)
            ++fStrums;
    }

    // st_release: key s goes up, its note ends and its waiting strings are dropped
    template <class Send>
    void release(uint32_t ofs, int s, Send& send) noexcept
    {
        if (fHeld[s] <= 0)
            return;
        fHeld[s] = 0;
        fNheld -= 1.0;
        drop(s);
        const int ch = s / 128, nt = s - ch * 128;
        if (fSnd[s] != 0.0)
        {
            send(ofs, 0x80 | ch, nt, 0);
            fSnd[s] = 0.0;
        }
        if (fNheld <= 0.0)
        {
            fNheld = 0.0;
            fRunning = false;
            fStrumAt = -1.0;
        }
    }

    // st_flush: all notes off on channel ch; its keys go up, its notes end
    template <class Send>
    void flush(uint32_t ofs, int ch, Send& send) noexcept
    {
        drop(-1 - ch);
        for (int nt = 0; nt < 128; ++nt)
        {
            const int s = ch * 128 + nt;
            if (fHeld[s] > 0)
            {
                fHeld[s] = 0;
                fNheld -= 1.0;
            }
            if (fSnd[s] != 0.0)
            {
                send(ofs, 0x80 | ch, nt, 0);
                fSnd[s] = 0.0;
            }
        }
        if (fNheld <= 0.0)
        {
            fNheld = 0.0;
            fRunning = false;
            fStrumAt = -1.0;
        }
    }

    double fSr = 48000.0;
    Params fP;
    double fGate = 0.85, fSpread = 0.25, fUpv = 0.8;
    double fSpd = 0.0, fHum = 0.0, fCol = 0.0, fStep = 1.0, fLen = 1.0;
    Ev fE[kEMax + 1] = {};
    int fEh = 0, fEt = 0;
    double fSnd[2048] = {};
    int fHeld[2048] = {};
    int fLst[2048] = {};
    double fNheld = 0.0, fNextId = 1.0, fNow = 0.0, fStrumAt = -1.0, fPat = 0.0, fSeed = 12345.0;
    bool fRunning = false;
    int fAlt = 0, fStrums = 0;
    Msg fQ[kQueue] = {};
    int fQn = 0, fQi = 0;
    uint32_t fSi = 0, fFrames = 0;
};

} // namespace nullstrum
