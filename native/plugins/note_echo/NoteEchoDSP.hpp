/*
 * NULL JSFX - Note Echo, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/note_echo.jsfx, computed in double in the EEL2 order: every
 * note-on puts Repeats echoes (and their note-offs) in a queue sorted by time; echo k has the
 * velocity v (1 - Velocity Decay)^k and the pitch moved by k x Pitch Step. A note that starts
 * again while it sounds is ended first; a note-off ends a note only when it is still the one
 * that note started (ids). Per sample, the echoes that are due go out first, then the MIDI that
 * came in at that sample; send(ofs, a, b, c) gets them in that order. The queue and the tables
 * are members, so nothing is allocated while it plays.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullnoteecho {

struct Params {
    int repeats = 3, sync = 1, subdivision = 1, dry = 1;
    double time = 250.0, decay = 20.0, pitchStep = 0.0, gate = 50.0;
};

class NoteEchoProcessor {
public:
    static constexpr int kQueue = 256;
    static constexpr int kEMax = 4096;

    void setSampleRate(double sr) noexcept { fSr = sr > 0.0 ? sr : 48000.0; }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fDec = p.decay / 100.0;
        fGate = p.gate / 100.0;
    }

    // @init
    void reset() noexcept
    {
        fQn = fQi = 0;
        fEh = fEt = 0;
        std::memset(fSnd, 0, sizeof(fSnd));
        std::memset(fOrg, 0, sizeof(fOrg));
        fNextId = 1.0;
        fNow = 0.0;
        fEchoes = 0;
    }

    // @block: the tempo, then the messages that came in (offsets clamped to the block)
    void begin(uint32_t frames, double tempo) noexcept
    {
        fQn = fQi = 0;
        fFrames = frames;
        const int s = fP.subdivision;
        double iv = fP.sync ? 60.0 / tempo * (s == 0 ? 1.0 : s == 1 ? 0.5 : s == 2 ? 0.25 : s == 3 ? 0.125 :
                                              s == 4 ? 2.0 : s == 5 ? 1.0 / 3.0 : s == 6 ? 1.0 / 6.0 : 0.75)
                            : fP.time / 1000.0;
        fIv = std::fmax(1.0, std::floor(iv * fSr));
        fLen = std::fmax(1.0, std::floor(fIv * fGate));
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

    int echoes() const noexcept { return fEchoes; }
    double interval() const noexcept { return fIv; }

    // one sample of @sample (the audio passes through untouched)
    template <class Send>
    void processSample(Send&& send) noexcept
    {
        const uint32_t si = fSi;
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
        while (fQi < fQn && fQ[fQi].ofs <= si)
        {
            const Msg& m = fQ[fQi];
            const int st = m.a & 0xF0;
            const int ch = m.a & 15;
            if (st == 0x90 && m.c > 0)
            {
                if (fP.dry)
                {
                    noteOn(si, ch, m.b, m.c, fNextId, send);
                    fOrg[ch * 128 + m.b] = fNextId;
                    fNextId += 1.0;
                }
                note(fNow, ch, m.b, m.c);
            }
            else if (st == 0x80 || st == 0x90)
            {
                if (fP.dry)
                    noteOff(si, ch, m.b, fOrg[ch * 128 + m.b], send);
            }
            else
            {
                if (st == 0xB0 && (m.b == 120 || m.b == 123))
                    flush(si, ch, send);
                send(si, m.a, m.b, m.c);
            }
            ++fQi;
        }
        fNow += 1.0;
        ++fSi;
    }

private:
    struct Msg { uint32_t ofs; int a, b, c; };
    struct Ev { double t; int st, nt, v; double id; };

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

    // ne_push: a waiting event, kept in time order (after the ones at the same time)
    void push(double t, int st, int nt, int v, double id) noexcept
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
        fE[i] = { t, st, nt, v, id };
        ++fEt;
    }

    // ne_note: a note-on that came in at time t; its echoes wait in the queue
    void note(double t, int ch, int nt, int v) noexcept
    {
        int k = 1;
        bool ok = true;
        while (ok && k <= fP.repeats)
        {
            const double vk = std::floor(v * std::pow(1.0 - fDec, double(k)) + 0.5);
            const double pk = nt + k * fP.pitchStep;
            if (vk >= 1.0 && pk >= 0.0 && pk <= 127.0 && fEt - fEh + 2 <= kEMax)
            {
                push(t + k * fIv, 0x90 | ch, int(pk), int(vk), fNextId);
                push(t + k * fIv + fLen, 0x80 | ch, int(pk), 0, fNextId);
                fNextId += 1.0;
                ++fEchoes;
            }
            else
                ok = false;
            ++k;
        }
    }

    // ne_flush: all notes off on channel ch; its echoes are dropped and its notes end
    template <class Send>
    void flush(uint32_t ofs, int ch, Send& send) noexcept
    {
        int j = fEh;
        for (int i = fEh; i < fEt; ++i)
            if ((fE[i].st & 15) != ch)
            {
                if (j != i)
                    fE[j] = fE[i];
                ++j;
            }
        fEt = j;
        for (int nt = 0; nt < 128; ++nt)
        {
            if (fSnd[ch * 128 + nt] != 0.0)
            {
                send(ofs, 0x80 | ch, nt, 0);
                fSnd[ch * 128 + nt] = 0.0;
            }
            fOrg[ch * 128 + nt] = 0.0;
        }
    }

    double fSr = 48000.0;
    Params fP;
    double fDec = 0.2, fGate = 0.5, fIv = 1.0, fLen = 1.0;
    Ev fE[kEMax + 1] = {};
    int fEh = 0, fEt = 0;
    double fSnd[2048] = {}, fOrg[2048] = {};
    double fNextId = 1.0, fNow = 0.0;
    int fEchoes = 0;
    Msg fQ[kQueue] = {};
    int fQn = 0, fQi = 0;
    uint32_t fSi = 0, fFrames = 0;
};

} // namespace nullnoteecho
