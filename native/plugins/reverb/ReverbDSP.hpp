/*
 * NULL JSFX - Reverb, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/reverb.jsfx, computed in double like EEL2: a stereo
 * Schroeder/Freeverb-style reverb. The mono sum feeds four damped feedback combs per side
 * (the right side's delays 23 samples longer at 44.1 kHz), then two series all-passes per
 * side; all lengths scale with srate / 44100. Damping is a one-pole low-pass in each comb's
 * feedback loop (16 kHz at 0% down to 1.2 kHz at 100%).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullreverb {

static constexpr int kSpread = 23;
static constexpr int kCombLen[4] = { 1557, 1617, 1491, 1687 };
static constexpr int kApLen[2] = { 556, 441 };

// the settings the JSFX's @slider derives (also used by the interface)
struct Params {
    double room, fb, dampFc, damp, dampA, mix;
};

inline Params paramsFor(double sizePct, double decayPct, double dampingPct, double mixPct, double srate) noexcept
{
    Params p;
    p.room = 0.25 + 0.75 * (sizePct / 100.0);
    p.fb = 0.7 + 0.27 * (decayPct / 100.0);
    p.dampFc = std::min(16000.0 * std::pow(1200.0 / 16000.0, dampingPct / 100.0), 0.45 * srate);
    p.damp = std::exp(-2.0 * 3.14159265358979 * p.dampFc / srate);
    p.dampA = 1.0 - p.damp;
    p.mix = mixPct / 100.0;
    return p;
}

class ReverbProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSc = sr / 44100.0;
        fBlen = int(std::ceil((1687 + kSpread) * fSc)) + 4;
        for (auto& c : fCombs)
            c.reset(fBlen);
        for (auto& a : fAps)
            a.reset(fBlen);
    }

    // @slider
    void setParams(double sizePct, double decayPct, double dampingPct, double mixPct, bool bypass)
    {
        fP = paramsFor(sizePct, decayPct, dampingPct, mixPct, fSr);
        const double k = fP.room * fSc;
        for (int i = 0; i < 4; ++i)
        {
            fCombs[i].setLen(int(std::max(1.0, std::floor(kCombLen[i] * k))));
            fCombs[4 + i].setLen(int(std::max(1.0, std::floor((kCombLen[i] + kSpread) * k))));
        }
        for (int i = 0; i < 2; ++i)
        {
            fAps[i].setLen(int(std::max(1.0, std::floor(kApLen[i] * k))));
            fAps[2 + i].setLen(int(std::max(1.0, std::floor((kApLen[i] + kSpread) * k))));
        }
        fBypass = bypass;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        const double fb = fP.fb, damp = fP.damp, dampA = fP.dampA, mix = fP.mix;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double origL = inL[i], origR = inR[i];
            const double x = (origL + origR) * 0.015;
            double wl = fCombs[0].tick(x, fb, damp, dampA) + fCombs[1].tick(x, fb, damp, dampA);
            wl = wl + fCombs[2].tick(x, fb, damp, dampA);
            wl = wl + fCombs[3].tick(x, fb, damp, dampA);
            double wr = fCombs[4].tick(x, fb, damp, dampA) + fCombs[5].tick(x, fb, damp, dampA);
            wr = wr + fCombs[6].tick(x, fb, damp, dampA);
            wr = wr + fCombs[7].tick(x, fb, damp, dampA);
            wl = fAps[1].tick(fAps[0].tick(wl));
            wr = fAps[3].tick(fAps[2].tick(wr));
            const double ol = origL * (1.0 - mix) + wl * 3.0 * mix;
            const double orr = origR * (1.0 - mix) + wr * 3.0 * mix;
            outL[i] = static_cast<T>(fBypass ? origL : ol);
            outR[i] = static_cast<T>(fBypass ? origR : orr);
        }
    }

private:
    struct Line {
        std::vector<double> buf;
        int len = 1, w = 0;
        double f = 0.0;
        void reset(int size)
        {
            buf.assign(size_t(size), 0.0);
            w = 0;
            f = 0.0;
        }
        void setLen(int l)
        {
            len = l;
            if (w >= len)
                w = 0;
        }
        // the JSFX's comb()
        double tick(double x, double fb, double damp, double dampA) noexcept
        {
            const double y = buf[size_t(w)];
            f = y * dampA + f * damp;
            buf[size_t(w)] = x + f * fb;
            if (++w >= len)
                w = 0;
            return y;
        }
        // the JSFX's allpass()
        double tick(double x) noexcept
        {
            const double b = buf[size_t(w)];
            const double y = b - x;
            buf[size_t(w)] = x + b * 0.5;
            if (++w >= len)
                w = 0;
            return y;
        }
    };

    double fSr = 48000.0, fSc = 48000.0 / 44100.0;
    int fBlen = 1;
    Line fCombs[8], fAps[4];
    Params fP {};
    bool fBypass = false;
};

} // namespace nullreverb
