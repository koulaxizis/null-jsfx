/*
 * NULL JSFX - the note tracking of the MIDI effects (Chords, Scale, MIDI Utility), framework-free: the same
 * steps as the cd_* / sc_* functions of chords.jsfx and scale.jsfx, so the MIDI that goes out is
 * the same message for message, at the same sample.
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Every note-on that goes out is remembered with the key (input channel * 128 + note) that made
 * it, so the key's note-off ends exactly those notes; notes still waiting (a strum) when the key
 * is released are never sent. Outgoing messages wait in a queue ordered by time (in samples
 * since the start); a pitch several keys hold starts again on each and ends with the last one.
 * All notes off (CC 120/123) releases every key of its channel first.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace nulljsfx {

class MidiNotes
{
public:
    static constexpr int kKeys = 16 * 128;
    static constexpr int kPerKey = 8;
    static constexpr int kQueue = 1024;

    struct Msg { double t; int a, b, c, s; };

    void reset()
    {
        std::memset(fAct, 0, sizeof(fAct));
        std::memset(fCount, 0, sizeof(fCount));
        std::memset(fCnt, 0, sizeof(fCnt));
        for (int i = 0; i < kKeys * kPerKey; ++i)
            fNote[i] = 0, fOn[i] = 0.0;
        fQn = 0;
        fPos = 0.0;
    }

    double position() const { return fPos; }
    bool held(int s) const { return fAct[s] != 0; }
    // 1 while some key holds output note (channel * 128 + note)
    bool sounding(int k) const { return fCnt[k] > 0; }

    // one incoming message at time t; onNote(t, ch, note, velocity, key) is called for a
    // note-on after the key's previous notes (if any) were released, and must call start()
    template <class OnNote>
    void input(double t, int a, int b, int c, OnNote&& onNote)
    {
        const int st = a & 0xF0, ch = a & 0x0F, s = ch * 128 + b;
        if (st == 0x90 && c > 0)
        {
            if (fAct[s])
                release(t, s, 0);
            onNote(t, ch, b, c, s);
        }
        else if (st == 0x80 || st == 0x90)
        {
            if (fAct[s])
                release(t, s, st == 0x80 ? c : 0);
        }
        else if (st == 0xA0)
        {
            for (int i = 0; i < fCount[s]; ++i)
                if (fOn[s * kPerKey + i] <= t)
                    put(t, a, fNote[s * kPerKey + i], c, -1);
        }
        else if (st == 0xB0 && (b == 120 || b == 123))
        {
            for (int i = 0; i < 128; ++i)
                if (fAct[ch * 128 + i])
                    release(t, ch * 128 + i, 0);
            put(t, a, b, c, -1);
        }
        else
            put(t, a, b, c, -1);
    }

    // key s sounds notes[0..count-1]: note i starts at t + i * strum (in this order)
    void start(double t, int s, const int* notes, int count, int vel, double strum)
    {
        fAct[s] = 1;
        fCount[s] = count;
        const int ch = s / 128;
        for (int i = 0; i < count; ++i)
        {
            fNote[s * kPerKey + i] = notes[i];
            fOn[s * kPerKey + i] = t + i * strum;
            put(t + i * strum, 0x90 | ch, notes[i], vel, s);
        }
    }

    // every key held ends at time t (MIDI Utility's new Channel)
    void releaseAll(double t)
    {
        for (int s = 0; s < kKeys; ++s)
            if (fAct[s])
                release(t, s, 0);
    }

    // the messages due in the next `frames` samples, in order: send(offset, status, d1, d2)
    template <class Send>
    void emit(uint32_t frames, Send&& send)
    {
        const double end = fPos + frames;
        int i = 0;
        for (; i < fQn && fQ[i].t < end; ++i)
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
        fPos = end;
    }

    // the size of a MIDI message by its status, as ysfx (and REAPER) send it
    static uint32_t size(int status)
    {
        switch (status & 0xF0)
        {
        case 0xC0: case 0xD0: return 2;
        case 0xF0:
            switch (status)
            {
            case 0xF1: case 0xF3: return 2;
            case 0xF2: return 3;
            case 0xF6: case 0xF8: case 0xFA: case 0xFB: case 0xFC: case 0xFE: case 0xFF: return 1;
            default: return 3;
            }
        default: return 3;
        }
    }

private:
    void put(double t, int a, int b, int c, int s)
    {
        if (fQn >= kQueue)
            return;
        int i = fQn;
        while (i > 0 && fQ[i - 1].t > t)
        {
            fQ[i] = fQ[i - 1];
            --i;
        }
        fQ[i] = Msg { t, a, b, c, s };
        ++fQn;
    }

    void drop(int s, int p)
    {
        int f = -1;
        for (int i = 0; f < 0 && i < fQn; ++i)
            if (fQ[i].s == s && fQ[i].b == p && (fQ[i].a & 0xF0) == 0x90)
                f = i;
        if (f < 0)
            return;
        for (int i = f; i + 1 < fQn; ++i)
            fQ[i] = fQ[i + 1];
        --fQn;
    }

    void release(double t, int s, int v)
    {
        for (int i = 0; i < fCount[s]; ++i)
        {
            const int p = fNote[s * kPerKey + i];
            if (fOn[s * kPerKey + i] <= t)
                put(t, 0x80 | (s / 128), p, v, s);
            else
                drop(s, p);
        }
        fAct[s] = 0;
        fCount[s] = 0;
    }

    uint8_t fAct[kKeys] = {};
    int fCount[kKeys] = {};
    int fNote[kKeys * kPerKey] = {};
    double fOn[kKeys * kPerKey] = {};
    int fCnt[kKeys] = {};
    Msg fQ[kQueue] = {};
    int fQn = 0;
    double fPos = 0.0;
};

} // namespace nulljsfx
