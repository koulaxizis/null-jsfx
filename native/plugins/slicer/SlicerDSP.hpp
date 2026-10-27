/*
 * NULL JSFX - Slicer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/slicer.jsfx, computed in double like EEL2. Each bar (4 beats)
 * of input is recorded into one of two bar buffers while the other, the previous bar, plays
 * cut into 8, 16 or 32 slices in the order the Pattern gives (see makePattern), each slice
 * possibly reversed or retriggered (its first half twice), shortened by Gate, with 2.5 ms fades
 * wherever the audio does not continue seamlessly. The choices come from a Park-Miller generator
 * in exact double arithmetic seeded from Seed. Until the first whole bar is recorded the input
 * passes through. While the transport plays the bar is locked to the song position (re-read
 * every block, as the JSFX's @block does); stopped, it free-runs at the tempo (40..300 bpm).
 */

#pragma once

#include "NullLazyClear.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullslicer {

struct Params {
    int pattern = 1;         // 0..5: Straight, Shuffle, Reverse Pairs, Stutter, Backspin, Random
    double variation = 50.0; // %
    double seed = 1.0;       // 1..999
    int slices = 1;          // 0..2: 8, 16, 32
    double reversePct = 0.0, repeatPct = 25.0, gatePct = 100.0, mixPct = 100.0;
};

// the bar's slices as the JSFX's @slider builds them
struct Pattern {
    int ns = 16;
    double src[32] = {}, rv[32] = {}, rp[32] = {}, bs[32] = {}, fi[32] = {}, fo[32] = {};
};

inline int sliceCount(int s) { return s == 0 ? 8 : s == 1 ? 16 : 32; }

inline Pattern makePattern(const Params& p)
{
    Pattern r;
    const int ns = sliceCount(p.slices);
    r.ns = ns;
    const double gate = p.gatePct / 100.0;
    double seed = p.seed * 7919.0 + 13.0;
    auto prand = [&seed](double x) {
        seed *= 16807.0;
        seed -= std::floor(seed / 2147483647.0) * 2147483647.0;
        return seed / 2147483648.0 * x;
    };
    for (int k = 0; k < 4; ++k)
        prand(1.0);
    double ua[32], up[32], ur[32], uq[32];
    for (int j = 0; j < ns; ++j)
    {
        ua[j] = prand(100.0);
        up[j] = prand(1.0);
        ur[j] = prand(100.0);
        uq[j] = prand(100.0);
        r.src[j] = j;
        r.rv[j] = r.rp[j] = r.bs[j] = 0.0;
    }
    if (p.pattern == 1)
    {
        // shuffle the picked slices among themselves (Fisher-Yates)
        int lst[32], prm[32], m = 0;
        for (int j = 0; j < ns; ++j)
            if (ua[j] < p.variation)
            {
                lst[m] = j;
                prm[m] = j;
                ++m;
            }
        for (int i = m - 1; i >= 1; --i)
        {
            const int k = int(std::floor(up[lst[i]] * (i + 1)));
            const int t = prm[i];
            prm[i] = prm[k];
            prm[k] = t;
        }
        for (int i = 0; i < m; ++i)
            r.src[lst[i]] = prm[i];
    }
    else if (p.pattern == 2)
    {
        // picked pairs play backwards as one piece
        for (int j = 0; j < ns; j += 2)
            if (ua[j] < p.variation)
            {
                r.src[j] = j + 1;
                r.src[j + 1] = j;
                r.rv[j] = r.rv[j + 1] = 1.0;
            }
    }
    else if (p.pattern == 3)
    {
        for (int j = 1; j < ns; ++j)
            if (ua[j] < p.variation)
                r.src[j] = r.src[j - 1];
    }
    else if (p.pattern == 4)
    {
        for (int j = 1; j < ns; ++j)
            if (ua[j] < p.variation)
                r.bs[j] = 1.0;
    }
    else if (p.pattern == 5)
    {
        for (int j = 0; j < ns; ++j)
            if (ua[j] < p.variation)
                r.src[j] = std::floor(up[j] * ns);
    }
    for (int j = 0; j < ns; ++j)
    {
        if (r.bs[j] != 0.0)
            r.rv[j] = 0.0;
        else
        {
            if (ur[j] < p.reversePct)
                r.rv[j] = 1.0;
            r.rp[j] = uq[j] < p.repeatPct ? 1.0 : 0.0;
        }
    }
    // fades only where the audio does not simply continue from the slice before
    for (int j = 0; j < ns; ++j)
    {
        const int k = j == 0 ? ns - 1 : j - 1;
        const bool c = gate >= 1.0 && r.bs[j] == 0.0 && r.bs[k] == 0.0 && r.rp[j] == 0.0 && r.rp[k] == 0.0 &&
                       ((r.rv[j] == 0.0 && r.rv[k] == 0.0 && r.src[j] == r.src[k] + 1.0 - (j == 0 ? ns : 0)) ||
                        (j > 0 && r.rv[j] != 0.0 && r.rv[k] != 0.0 && r.src[j] == r.src[k] - 1.0));
        r.fi[j] = c ? 0.0 : 1.0;
        r.fo[k] = c ? 0.0 : 1.0;
    }
    return r;
}

class SlicerProcessor {
public:
    // @init: the bar buffers are allocated here (cleared), never on the audio thread
    void setSampleRate(double sr)
    {
        fSr = sr;
        fFd = std::fmax(1.0, std::floor(0.0025 * fSr));
        fBmax = std::ceil(fSr * 6.0) + 4.0;
        for (int c = 0; c < 2; ++c)
        {
            fL[c].allocate(size_t(fBmax));
            fR[c].allocate(size_t(fBmax));
        }
        reset();
    }

    // @init again (REAPER re-runs it when the transport starts). The bar buffers read as cleared
    // at once but are cleared chunk by chunk as they are written (NullLazyClear.hpp), so a
    // transport start costs no multi-millisecond clear on the audio thread.
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            fL[c].clearAll();
            fR[c].clearAll();
        }
        fN = 0.0;
        fCur = 0.0;
        fJf = 1.0;
        fFull = 0.0;
        fFl = 0.0;
        updateParams();
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    // for the display: the position in the bar (0..1) and whether a whole bar was recorded
    double barPos() const noexcept { return fN / std::fmax(1.0, fBl); }
    bool full() const noexcept { return fFull != 0.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo, bool playing,
                 double beatPos)
    {
        // @block
        double bpm = tempo > 0.0 ? tempo : 120.0;
        bpm = std::fmin(300.0, std::fmax(40.0, bpm));
        const double spb = fSr * 60.0 / bpm;
        fBl = 4.0 * spb;
        const double sl = fBl / fPat.ns;
        if (playing)
        {
            const double bar = std::floor(beatPos / 4.0);
            const double nt = std::floor((beatPos - 4.0 * bar) * spb + 0.5);
            const double ct = bar - 2.0 * std::floor(bar / 2.0);
            const double at = ct * fBl + nt;
            const double a = fCur * fBl + fN;
            double d = at - a;
            if (d > fBl)
                d -= 2.0 * fBl;
            else if (d < -fBl)
                d += 2.0 * fBl;
            if (std::fabs(d) > 2.0)
            {
                fN = nt;
                fCur = ct;
                fJf = 0.0;
            }
            else if (std::fabs(at - a) <= 2.0)
            {
                fN = nt;
                fCur = ct;
            }
        }

        const double ns = fPat.ns;
        for (uint32_t s = 0; s < frames; ++s)
        {
            const double x0 = inL[s], x1 = inR[s];
            if (fN >= fBl)
            {
                fN = 0.0;
                fCur = 1.0 - fCur;
                fFull = 1.0;
            }
            const int wc = fCur != 0.0 ? 1 : 0;
            if (fN < fBmax - 2.0)
            {
                fL[wc].set(size_t(fN), x0);
                fR[wc].set(size_t(fN), x1);
            }
            const double jd = std::fmin(ns - 1.0, std::floor(fN / sl));
            const int j = int(jd);
            const double o = fN - jd * sl;
            const double ae = fGate * sl;
            double p, env;
            if (fPat.bs[j] != 0.0)
            {
                // backspin: rewind from the slice start, speeding up from 1x to 3x
                p = jd * sl - (o + o * o / sl);
                env = std::fmin(std::fmin(1.0, o / fFd), (ae - o) / fFd);
            }
            else if (fPat.rp[j] != 0.0)
            {
                // repeat: the first half of the slice twice
                const double h = sl / 2.0;
                double oo;
                if (o < h)
                {
                    oo = o;
                    env = std::fmin(fPat.fi[j] != 0.0 ? o / fFd : 1.0, (std::fmin(h, ae) - o) / fFd);
                }
                else
                {
                    oo = o - h;
                    env = std::fmin((o - h) / fFd, (ae - o) / fFd);
                }
                p = fPat.rv[j] != 0.0 ? fPat.src[j] * sl + sl - 1.0 - oo : fPat.src[j] * sl + oo;
            }
            else
            {
                p = fPat.rv[j] != 0.0 ? fPat.src[j] * sl + sl - 1.0 - o : fPat.src[j] * sl + o;
                env = std::fmin(fPat.fi[j] != 0.0 ? o / fFd : 1.0,
                                fPat.fo[j] != 0.0 || fGate < 1.0 ? (ae - o) / fFd : 1.0);
            }
            env = std::fmax(0.0, std::fmin(1.0, env)) * fJf;
            if (fJf < 1.0)
                fJf = std::fmin(1.0, fJf + 1.0 / fFd);
            p = std::fmax(0.0, std::fmin(fBmax - 2.0, p));
            const int rc = 1 - wc;
            double wl = rd(fL[rc], p) * env;
            double wr = rd(fR[rc], p) * env;
            if (fFl < 1.0)
            {
                if (fFull != 0.0)
                    fFl = std::fmin(1.0, fFl + 1.0 / fFd);
                wl = wl * fFl + x0 * (1.0 - fFl);
                wr = wr * fFl + x1 * (1.0 - fFl);
            }
            outL[s] = static_cast<T>(x0 * (1.0 - fMix) + wl * fMix);
            outR[s] = static_cast<T>(x1 * (1.0 - fMix) + wr * fMix);
            fN += 1.0;
        }
    }

private:
    static double rd(const nulljsfx::LazyClearBuffer& b, double p)
    {
        const double i = std::floor(p);
        const double f = p - i;
        const double v = b.get(size_t(i));
        return v + (b.get(size_t(i) + 1) - v) * f;
    }

    void updateParams()
    {
        fPat = makePattern(fP);
        fGate = fP.gatePct / 100.0;
        fMix = fP.mixPct / 100.0;
    }

    Params fP;
    Pattern fPat;
    double fSr = 48000.0, fFd = 1.0, fBmax = 4.0, fBl = 1.0;
    nulljsfx::LazyClearBuffer fL[2], fR[2];
    double fN = 0.0, fCur = 0.0, fJf = 1.0, fFull = 0.0, fFl = 0.0, fGate = 1.0, fMix = 1.0;
};

} // namespace nullslicer
