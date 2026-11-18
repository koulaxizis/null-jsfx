/*
 * NULL JSFX - Beat Gen, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/beat_gen.jsfx, computed in double in the EEL2 order: 8 drum lanes
 * of up to 16 steps drawn from Style, Seed and each lane's Density (makePattern, exact Park-Miller
 * in doubles), one global step per Rate on the host's beat (or a free-running beat at the host's
 * tempo with Run Always); each lane plays step (global step mod its Steps). Mutate plays a
 * variation every Mutate Every cycles of 16 steps (mutateInto). Per sample, the MIDI that came in
 * at that sample goes out first, then the note-offs that are due, then the notes of a step;
 * send(ofs, a, b, c) gets them in that order.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullbeatgen {

static constexpr int kLanes = 8;

struct Params {
    double density[kLanes] = { 50, 40, 70, 15, 15, 0, 10, 0 };
    int steps[kLanes] = { 16, 16, 16, 16, 16, 16, 16, 16 };
    int note[kLanes] = { 36, 38, 42, 46, 39, 45, 37, 51 };
    bool lock[kLanes] = {};
    int style = 0, rate = 1, mutEvery = 1, run = 0, channel = 9;
    double seed = 1.0, mutPct = 0.0, velocity = 100.0, velRange = 30.0, swing = 0.0, gate = 50.0;
};

// the styles' weights, a lane of 16 steps after another ('.' = never), as the JSFX's bg_wstr
inline const char* styleWeights(int st) noexcept
{
    static const char* const kW[] = {
    "9...9...9...9..3....9.......9...3.9.3.9.3.9.3.93..6...6...6...6.....9.......9..............3..53..3..3....3..3..9...............",
    "9.....6.3.9.........9..3..3.9..59595959595959595.......6.......6....5.......5...............5.533..3..3..3..3..36...............",
    "9......6.39.........9.......9...9.6.9.6.9.6.9.6...............5.....6.......6.................3....3.....3....3.5...............",
    "9.....6...9..3..........9.......9696969696969696...........5............9....................535..3.....3...3...5...............",
    "55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555",
    };
    return kW[st < 0 ? 0 : st > 4 ? 4 : st];
}

inline double weight(int st, int l, int i) noexcept
{
    const char c = styleWeights(st)[l * 16 + i];
    return c >= '1' && c <= '9' ? double(c - '0') / 9.0 : 0.0;
}

// one Park-Miller step (x * 16807 mod 2^31 - 1, exact in doubles), as the JSFX's bg_pm
inline double bgPm(double x) noexcept
{
    x *= 16807.0;
    return x - std::floor(x / 2147483647.0) * 2147483647.0;
}

// (x * y) mod 2147483647, exact in doubles (y split into 16-bit halves), as the JSFX's pm_mul
inline double pmMul(double x, double y) noexcept
{
    const double hi = std::floor(y / 65536.0);
    double r = x * hi;
    r -= std::floor(r / 2147483647.0) * 2147483647.0;
    r = r * 65536.0 + x * (y - hi * 65536.0);
    r -= std::floor(r / 2147483647.0) * 2147483647.0;
    if (r < 0.0)
        r += 2147483647.0;
    if (r >= 2147483647.0)
        r -= 2147483647.0;
    return r;
}

// the Park-Miller value (0..1) for step i of seed sd, as the JSFX's pm_step
inline double pmStep(double i, double sd) noexcept
{
    double e = i - std::floor(i / 2147483646.0) * 2147483646.0 + 8.0;
    double b = 16807.0, p = sd + 1.0;
    while (e > 0.0)
    {
        if (e - std::floor(e / 2.0) * 2.0 > 0.0)
            p = pmMul(p, b);
        b = pmMul(b, b);
        e = std::floor(e / 2.0);
    }
    return p / 2147483647.0;
}

// the chance that step i of lane l plays, as the JSFX's bg_prob
inline double prob(const Params& p, int l, int i) noexcept
{
    return std::fmin(1.0, weight(p.style, l, i) * 2.0 * p.density[l] / 100.0);
}

// the pattern and its velocities, as the JSFX's bg_make (always 2 draws per step)
inline void makePattern(const Params& p, bool* hit, int* vel) noexcept
{
    double x = p.seed * 31.0 + double(p.style) * 1000.0 + 777.0;
    for (int k = 0; k < 4; ++k)
        x = bgPm(x);
    const double vr = p.velRange / 100.0;
    for (int l = 0; l < kLanes; ++l)
        for (int i = 0; i < 16; ++i)
        {
            x = bgPm(x);
            hit[l * 16 + i] = x / 2147483647.0 < prob(p, l, i);
            x = bgPm(x);
            vel[l * 16 + i] = int(std::fmax(1.0, std::floor(p.velocity * (1.0 - vr * x / 2147483647.0) + 0.5)));
        }
}

// Mutate: the variation of cycle block m, as the JSFX's bg_mut
inline void mutateInto(const Params& p, double m, const bool* hit, bool* mh) noexcept
{
    const double mut = p.mutPct / 100.0;
    for (int l = 0; l < kLanes; ++l)
        for (int i = 0; i < 16; ++i)
        {
            const int k = l * 16 + i;
            if (m == 0.0 || p.lock[l])
                mh[k] = hit[k];
            else
            {
                const double base = m * 1024.0 + l * 64.0 + i * 2.0;
                mh[k] = pmStep(base + 7919.0, p.seed) < mut ? pmStep(base + 7920.0, p.seed) < prob(p, l, i) : hit[k];
            }
        }
}

inline int mutEveryCycles(int sel) noexcept { return sel == 0 ? 1 : sel == 1 ? 2 : sel == 2 ? 4 : 8; }

class BeatGenProcessor {
public:
    static constexpr int kQueue = 256;

    void setSampleRate(double sr) noexcept { fSr = sr > 0.0 ? sr : 48000.0; }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fSb = p.rate == 0 ? 0.5 : p.rate == 1 ? 0.25 : p.rate == 2 ? 0.125 : 1.0 / 6.0;
        fMut = p.mutPct / 100.0;
        fMev = mutEveryCycles(p.mutEvery);
        fSwd = p.swing / 100.0 * 0.25;
        fGate = p.gate / 100.0;
        makePattern(fP, fHit, fVel);
        fMbLast = -0.5;
    }

    // @init
    void reset() noexcept
    {
        fQn = fQi = 0;
        fLastS = -1000.0;
        fFb = 0.0;
        for (int l = 0; l < kLanes; ++l)
            fOffLeft[l] = 0, fOnNote[l] = 0, fOnChan[l] = 0;
        fCurS = -1.0;
        fWasRun = false;
        fMbLast = -0.5;
        fMbCur = 0.0;
    }

    // @block: the host's transport, then the messages that came in (offsets clamped to the block)
    void begin(uint32_t frames, bool playing, double tempo, double beat) noexcept
    {
        fQn = fQi = 0;
        fFrames = frames;
        fRun = playing || fP.run != 0;
        fBInc = tempo / (60.0 * fSr);
        fB0 = playing ? beat : fFb;
        fFb = fB0 + double(frames) * fBInc;
        fLen = int(std::fmax(1.0, std::floor(fGate * (1.0 - 2.0 * fSwd) * fSb * 60.0 / tempo * fSr)));
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

    double currentStep() const noexcept { return fCurS; }  // the global step that plays, -1 = none
    double mutationBlock() const noexcept { return fMbCur; }

    // one sample of @sample (the audio passes through untouched)
    template <class Send>
    void processSample(Send&& send) noexcept
    {
        const uint32_t si = fSi;
        while (fQi < fQn && fQ[fQi].ofs <= si)
        {
            const Msg& m = fQ[fQi];
            if ((m.a & 0xF0) == 0xB0 && (m.b == 120 || m.b == 123))
                for (int l = 0; l < kLanes; ++l)
                    noteOff(si, l, send);
            send(si, m.a, m.b, m.c);
            ++fQi;
        }
        for (int l = 0; l < kLanes; ++l)
            if (fOffLeft[l] > 0)
            {
                --fOffLeft[l];
                if (fOffLeft[l] == 0)
                    send(si, 0x80 | fOnChan[l], fOnNote[l], 0);
            }
        if (!fRun && fWasRun)
        {
            for (int l = 0; l < kLanes; ++l)
                noteOff(si, l, send);
            fLastS = -1000.0;
            fCurS = -1.0;
        }
        fWasRun = fRun;
        if (fRun)
        {
            const double bb = fB0 + double(si) * fBInc;
            if (bb >= 0.0)
            {
                const double u = bb / fSb;
                const double pp = std::floor(u / 2.0 + 0.000001);
                const double s = pp * 2.0 + (u / 2.0 - pp + 0.000001 >= 0.5 + fSwd ? 1.0 : 0.0);
                if (s != fLastS)
                {
                    const bool odd = (int64_t(s) & 1) != 0;
                    if (fLastS > -1000.0 || u - (odd ? 2.0 * pp + 1.0 + 2.0 * fSwd : 2.0 * pp) < 0.004)
                        step(si, s, send);
                    fLastS = s;
                }
            }
        }
        ++fSi;
    }

private:
    struct Msg { uint32_t ofs; int a, b, c; };

    template <class Send>
    void noteOff(uint32_t ofs, int l, Send& send) noexcept
    {
        if (fOffLeft[l] > 0)
        {
            send(ofs, 0x80 | fOnChan[l], fOnNote[l], 0);
            fOffLeft[l] = 0;
        }
    }

    // bg_step: global step s of the running position; every lane plays its own step of it
    template <class Send>
    void step(uint32_t ofs, double s, Send& send) noexcept
    {
        fCurS = s;
        if (fMut > 0.0)
        {
            fMbCur = std::floor(s / (16.0 * fMev));
            if (std::fabs(fMbCur - fMbLast) >= 0.00001) // EEL2 != is fuzzy
            {
                fMbLast = fMbCur;
                mutateInto(fP, fMbCur, fHit, fMh);
            }
        }
        for (int l = 0; l < kLanes; ++l)
        {
            const double n = double(fP.steps[l]);
            const int i = int(s - std::floor(s / n) * n + 0.0001);
            const int k = l * 16 + (i & 15);
            const bool h = fMut > 0.0 ? fMh[k] : fHit[k];
            if (h)
            {
                noteOff(ofs, l, send);
                send(ofs, 0x90 | fP.channel, fP.note[l], fVel[k]);
                fOnNote[l] = fP.note[l];
                fOnChan[l] = fP.channel;
                fOffLeft[l] = fLen;
            }
        }
    }

    double fSr = 48000.0;
    Params fP;
    double fSb = 0.25, fMut = 0.0, fSwd = 0.0, fGate = 0.5;
    int fMev = 2;
    bool fHit[kLanes * 16] = {}, fMh[kLanes * 16] = {};
    int fVel[kLanes * 16] = {};
    double fLastS = -1000.0, fFb = 0.0, fB0 = 0.0, fBInc = 0.0, fCurS = -1.0, fMbLast = -0.5, fMbCur = 0.0;
    int fOffLeft[kLanes] = {}, fOnNote[kLanes] = {}, fOnChan[kLanes] = {}, fLen = 1;
    bool fRun = false, fWasRun = false;
    Msg fQ[kQueue] = {};
    int fQn = 0, fQi = 0;
    uint32_t fSi = 0, fFrames = 0;
};

} // namespace nullbeatgen
