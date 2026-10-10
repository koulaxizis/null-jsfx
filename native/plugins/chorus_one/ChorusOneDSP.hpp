/*
 * NULL JSFX - Chorus One, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/chorus_one.jsfx, computed in double like EEL2 and in the
 * same order: a triangle LFO (0.3-2.8 Hz) moves two 5-12 ms delay lines in opposite
 * directions (1-8 ms depth), with a little feedback, mixed up to 85 % wet.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace nullchorusone {

struct Params {
    double chorusVal, lfoRate, lfoDepth, delayBase, feedback;
};

// the JSFX @slider section
inline Params paramsForAmount(double amount) noexcept
{
    const double c = amount / 100.0;
    return { c, 0.3 + c * 2.5, 1.0 + c * 7.0, 5.0 + c * 7.0, c * 0.15 };
}

// display: delay time (ms) of the left (sgn 1) or right (sgn -1) line at time t (s)
inline double viewDelayMs(const Params& p, double t, double sgn) noexcept
{
    double ph = t * p.lfoRate;
    ph -= std::floor(ph);
    const double tv = ph * 4.0 - 2.0;
    return p.delayBase + sgn * (std::fabs(tv) - 1.0) * p.lfoDepth;
}

class ChorusOneProcessor {
public:
    void setSampleRate(double sr)
    {
        fSr = sr;
        fBufSize = std::ceil(sr * 0.05);
        fBufL.assign(size_t(fBufSize), 0.0);
        fBufR.assign(size_t(fBufSize), 0.0);
        fMsToSamples = sr / 1000.0;
        reset();
    }

    void setAmount(double amount) noexcept
    {
        fP = paramsForAmount(amount);
        fMsToSamples = fSr / 1000.0;
    }

    // JSFX @init
    void reset() noexcept
    {
        std::fill(fBufL.begin(), fBufL.end(), 0.0);
        std::fill(fBufR.begin(), fBufR.end(), 0.0);
        fHeadL = fHeadR = 0.0;
        fPhase = 0.0;
        fFbL = fFbR = 0.0;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const double bs = fBufSize;
        const long ibs = long(bs);
        for (uint32_t i = 0; i < frames; ++i) {
            const double x0 = inL[i], x1 = inR[i];

            fPhase = fPhase + fP.lfoRate / fSr;
            if (fPhase >= 1.0)
                fPhase = fPhase - 1.0;
            const double tv = fPhase * 4.0 - 2.0;
            if (tv < 0.0)
                fLfo = -tv - 1.0;
            if (tv >= 0.0)
                fLfo = tv - 1.0;

            const double dlMs = fP.delayBase + fLfo * fP.lfoDepth;
            const double drMs = fP.delayBase - fLfo * fP.lfoDepth;
            const double dlS = dlMs * fMsToSamples;
            const double drS = drMs * fMsToSamples;

            const double outDl = line(fBufL, fHeadL, dlS, bs, ibs, x0 + fFbL * fP.feedback);
            fFbL = outDl;
            const double outDr = line(fBufR, fHeadR, drS, bs, ibs, x1 + fFbR * fP.feedback);
            fFbR = outDr;

            const double wet = fP.chorusVal * 0.85;
            const double dry = 1.0 - wet;
            outL[i] = static_cast<T>(x0 * dry + outDl * wet);
            outR[i] = static_cast<T>(x1 * dry + outDr * wet);
        }
    }

private:
    // one channel: advance the head, read with linear interpolation, then write
    static double line(std::vector<double>& buf, double& head, double ds, double bs, long ibs, double write) noexcept
    {
        const double di = std::floor(ds);
        const double frac = ds - di;
        head = double((long(head) + 1) % ibs); // EEL2 % works on integers
        double rp = head - di;
        if (rp < 0.0)
            rp = bs + rp;
        if (rp >= bs)
            rp = rp - bs;
        double rp2 = rp - 1.0;
        if (rp2 < 0.0)
            rp2 = bs + rp2;
        const double out = buf[size_t(rp)] * (1.0 - frac) + buf[size_t(rp2)] * frac;
        buf[size_t(head)] = write;
        return out;
    }

    double fSr = 48000.0, fBufSize = 2400.0, fMsToSamples = 48.0;
    Params fP = paramsForAmount(50.0);
    std::vector<double> fBufL, fBufR;
    double fHeadL = 0.0, fHeadR = 0.0, fPhase = 0.0, fLfo = 0.0, fFbL = 0.0, fFbR = 0.0;
};

} // namespace nullchorusone
