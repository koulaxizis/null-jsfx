/*
 * NULL JSFX - Humanize (VST3/CLAP): the MIDI processing of humanize.jsfx, framework-free, the
 * same steps as its hu_* functions so the MIDI that goes out is the same message for message,
 * at the same sample
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nullhumanize {

struct Params {
    double timing = 10.0, velocity = 10.0, amount = 50.0, seed = 1.0;
    int groove = 0;
};

// one note for the display: beat in, beat out, velocity in, velocity out
struct HistoryNote { double in, out, vel, velOut; };

class HumanizeProcessor
{
public:
    static constexpr int kKeys = 16 * 128;
    static constexpr int kQueue = 2048;
    static constexpr int kHistory = 16;

    struct Msg { double t; int a, b, c; };

    // the audio delay line is sized here (never in process): the JSFX's DN
    void setSampleRate(double sr)
    {
        fSr = sr;
        fDn = int(std::floor(sr * 0.06)) + 2;
        fDl.assign(size_t(fDn) * 2, 0.0);
    }

    // @init
    void reset()
    {
        std::memset(fAct, 0, sizeof(fAct));
        std::memset(fCnt, 0, sizeof(fCnt));
        for (int i = 0; i < kKeys; ++i)
            fOnT[i] = 0.0;
        std::memset(fHist, 0, sizeof(fHist));
        fQn = 0;
        fPos = 0.0;
        fFb = 0.0;
        fWasPlay = false;
        fPm = 1.0;
        fPmSeed = -1.0;
        fHi = 0;
        fHn = 0;
        std::fill(fDl.begin(), fDl.end(), 0.0);
        fDw = 0;
    }

    // @sample: the audio passes through delayed by the latency (in place is fine)
    void audio(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
    {
        const int lat = int(fLat);
        for (uint32_t i = 0; i < frames; ++i)
        {
            fDl[size_t(fDw) * 2] = inL[i];
            fDl[size_t(fDw) * 2 + 1] = inR[i];
            int dr = fDw - lat;
            if (dr < 0)
                dr += fDn;
            outL[i] = float(fDl[size_t(dr) * 2]);
            outR[i] = float(fDl[size_t(dr) * 2 + 1]);
            if (++fDw >= fDn)
                fDw = 0;
        }
    }

    // @slider
    void setParams(const Params& p)
    {
        fTm = p.timing;
        fVa = p.velocity / 100.0;
        fGrv = p.groove;
        fGa = p.amount / 100.0;
        fSeed = p.seed;
    }

    // the latency (samples) for these settings at this sample rate, as the JSFX's pdc_delay
    double latency() const
    {
        double ts = fTm * fSr;
        ts /= 1000.0;
        double gs = 25.0 * fGa;
        gs *= fSr;
        gs /= 1000.0;
        const double ps = fGrv == 4 ? gs : 0.0;
        return std::floor(ts + ps + 0.5);
    }

    // @block, before the messages
    void begin(bool playing, double tempo, double beat)
    {
        fBps = 60.0 * fSr;
        fBps = std::fmax(1.0, tempo) / fBps;
        if ((playing && !fWasPlay) || fSeed != fPmSeed)
        {
            fPmSeed = fSeed;
            fPm = 12345.0 + fSeed * 7919.0;
        }
        fWasPlay = playing;
        fTs = fTm * fSr;
        fTs /= 1000.0;
        fGs = 25.0 * fGa;
        fGs *= fSr;
        fGs /= 1000.0;
        fPs = fGrv == 4 ? fGs : 0.0;
        fLat = std::floor(fTs + fPs + 0.5);
        fB0 = playing ? beat : fFb;
    }

    // one incoming message
    void input(uint32_t ofs, int a, int b, int c)
    {
        const int st = a & 0xF0, ch = a & 0x0F, s = ch * 128 + b;
        const double t = fPos + ofs;
        if (st == 0x90 && c > 0)
            noteOn(ofs, ch, b, c);
        else if (st == 0x80 || st == 0x90)
            noteOff(ofs, ch, b, st == 0x80 ? c : 0);
        else if (st == 0xA0)
            put(fAct[s] ? std::fmax(t + fLat, fOnT[s]) : t + fLat, a, b, c);
        else if (st == 0xB0 && (b == 120 || b == 123))
        {
            for (int i = 0; i < 128; ++i)
                if (fAct[ch * 128 + i])
                    noteOff(ofs, ch, i, 0);
            put(t + fLat, a, b, c);
        }
        else
            put(t + fLat, a, b, c);
    }

    // the rest of @block: the messages due: send(offset, status, d1, d2)
    template <class Send>
    void end(uint32_t frames, Send&& send)
    {
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

    // for the display: the last notes, oldest first (count, then each)
    int historyCount() const { return fHn; }
    const HistoryNote& history(int i) const { return fHist[(fHi - fHn + i + 2 * kHistory) % kHistory]; }

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

    double pmRand()
    {
        fPm *= 16807.0;
        fPm -= std::floor(fPm / 2147483647.0) * 2147483647.0;
        return fPm / 2147483647.0;
    }

    double groove(uint32_t ofs) const
    {
        if (fGrv == 1 || fGrv == 2)
        {
            const double p = fGrv == 1 ? 1.0 : 0.5;
            const double b = (fB0 + ofs * fBps) / p;
            const double x = b - std::floor(b);
            const double r = 0.5 + fGa / 6.0;
            double y = x < 0.5 ? x * 2.0 : (x - 0.5) * 2.0;
            y = x < 0.5 ? y * r : r + y * (1.0 - r);
            y = (y - x) * p;
            return y / fBps;
        }
        return fGrv == 3 ? fGs : fGrv == 4 ? -fPs : 0.0;
    }

    double offTime(uint32_t ofs, int s) const
    {
        return std::fmax(fPos + ofs + std::fmax(0.0, std::floor(fLat + groove(ofs) + 0.5)), fOnT[s]);
    }

    void noteOff(uint32_t ofs, int ch, int n, int v)
    {
        const int s = ch * 128 + n;
        if (!fAct[s])
            return;
        put(offTime(ofs, s), 0x80 | ch, n, v);
        fAct[s] = 0;
    }

    void noteOn(uint32_t ofs, int ch, int n, int v)
    {
        const int s = ch * 128 + n;
        if (fAct[s])
            noteOff(ofs, ch, n, 0);
        const double u = pmRand(), w = pmRand();
        const double d = std::fmax(0.0, std::floor(fLat + (2.0 * u - 1.0) * fTs + groove(ofs) + 0.5));
        const double vo = std::fmax(1.0, std::fmin(127.0, std::floor(v * (1.0 + fVa * (2.0 * w - 1.0)) + 0.5)));
        fAct[s] = 1;
        fOnT[s] = fPos + ofs + d;
        put(fPos + ofs + d, 0x90 | ch, n, int(vo));
        fHist[fHi] = HistoryNote { fB0 + ofs * fBps, fB0 + (ofs + d) * fBps, double(v), vo };
        fHi = (fHi + 1) % kHistory;
        fHn = fHn + 1 < kHistory ? fHn + 1 : kHistory;
    }

    double fSr = 44100.0, fTm = 10.0, fVa = 0.1, fGa = 0.5, fSeed = 1.0;
    int fGrv = 0;
    double fBps = 0.0, fB0 = 0.0, fFb = 0.0, fPos = 0.0, fPm = 1.0, fPmSeed = -1.0;
    double fTs = 0.0, fGs = 0.0, fPs = 0.0, fLat = 0.0;
    bool fWasPlay = false;
    uint8_t fAct[kKeys] = {};
    double fOnT[kKeys] = {};
    int fCnt[kKeys] = {};
    Msg fQ[kQueue] = {};
    int fQn = 0;
    HistoryNote fHist[kHistory] = {};
    int fHi = 0, fHn = 0;
    std::vector<double> fDl;
    int fDn = 2, fDw = 0;
};

} // namespace nullhumanize
