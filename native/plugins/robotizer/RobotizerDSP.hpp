/*
 * NULL JSFX - Robotizer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/robotizer.jsfx, computed in double like EEL2 (FFTs with
 * EelFft.hpp, EEL2's fft/ifft/fft_permute semantics): once every Pitch period the last 1024
 * input samples are Hann-windowed, their phases zeroed and the frame resynthesised as one
 * pulse, overlap-added into a 2048-sample output ring. A comb at the pitch period (Metallic),
 * 30 Hz ring modulation (Ring) and a dry/wet mix with the dry signal delayed by 1024 samples
 * (the reported latency) follow.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullrobotizer {

static constexpr int kN = 1024;   // frame (FFT) size and latency
static constexpr int kComb = 8192; // comb line length (the JSFX's CL)

// the pitch period in samples (the JSFX's P)
inline int period(double sr, double pitchHz) noexcept
{
    return int(std::max(16.0, std::floor(sr / pitchHz + 0.5)));
}

class RobotizerProcessor {
public:
    RobotizerProcessor()
        : fFft(2 * kN), fWin(kN), fInL(kN), fInR(kN), fOutL(2 * kN), fOutR(2 * kN), fCombL(kComb), fCombR(kComb)
    {
        for (int i = 0; i < kN; ++i)
            fWin[size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kN);
    }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (std::vector<double>* v : { &fInL, &fInR, &fOutL, &fOutR, &fCombL, &fCombR })
            std::fill(v->begin(), v->end(), 0.0);
        fIpos = fOpos = fCnt = fCpos = 0;
        fRph = 0.0;
    }

    // @slider
    void setParams(double pitchHz, double metallicPct, double ringPct, double mixPct)
    {
        fP = period(fSr, pitchHz);
        fNorm = double(fP) / kN * 2.0 / kN;
        fMetal = metallicPct / 100.0 * 0.85;
        fRing = ringPct / 100.0;
        fMix = mixPct / 100.0;
    }

    static constexpr int latency() noexcept { return kN; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            fInL[size_t(fIpos)] = inL[n];
            fInR[size_t(fIpos)] = inR[n];
            if (++fIpos >= kN)
                fIpos = 0;

            if (++fCnt >= fP)
            {
                fCnt = 0;
                frame(fInL, fOutL);
                frame(fInR, fOutR);
            }

            const double rl = fOutL[size_t(fOpos)], rr = fOutR[size_t(fOpos)];
            fOutL[size_t(fOpos)] = 0.0;
            fOutR[size_t(fOpos)] = 0.0;
            if (++fOpos >= 2 * kN)
                fOpos = 0;

            int cp = fCpos - fP;
            if (cp < 0)
                cp += kComb;
            double cl = rl + fMetal * fCombL[size_t(cp)];
            double cr = rr + fMetal * fCombR[size_t(cp)];
            fCombL[size_t(fCpos)] = cl;
            fCombR[size_t(fCpos)] = cr;
            if (++fCpos >= kComb)
                fCpos = 0;
            cl *= 1.0 - fMetal;
            cr *= 1.0 - fMetal;

            fRph += 30.0 / fSr;
            if (fRph >= 1.0)
                fRph -= 1.0;
            const double rm = std::sin(2.0 * M_PI * fRph);
            cl = cl * (1.0 - fRing) + cl * rm * fRing;
            cr = cr * (1.0 - fRing) + cr * rm * fRing;

            const double dl = fInL[size_t(fIpos)], dr = fInR[size_t(fIpos)];
            outL[n] = static_cast<T>(dl * (1.0 - fMix) + cl * fMix);
            outR[n] = static_cast<T>(dr * (1.0 - fMix) + cr * fMix);
        }
    }

private:
    // robot_frame: zero-phase resynthesis of the last N samples, added at the read point
    void frame(const std::vector<double>& inb, std::vector<double>& outb)
    {
        double* b = fFft.data();
        for (int i = 0; i < kN; ++i)
        {
            b[2 * i] = inb[size_t((fIpos + i) % kN)] * fWin[size_t(i)];
            b[2 * i + 1] = 0.0;
        }
        eelfft::fft(b, kN);
        eelfft::fftPermute(b, kN);
        for (int k = 0; k < kN; ++k)
        {
            const double re = b[2 * k], im = b[2 * k + 1];
            const double m = std::sqrt(re * re + im * im);
            b[2 * k] = (k & 1) ? -m : m;
            b[2 * k + 1] = 0.0;
        }
        eelfft::fftIpermute(b, kN);
        eelfft::ifft(b, kN);
        for (int i = 0; i < kN; ++i)
            outb[size_t((fOpos + i) % (2 * kN))] += b[2 * i] * fNorm;
    }

    double fSr = 48000.0;
    std::vector<double> fFft, fWin, fInL, fInR, fOutL, fOutR, fCombL, fCombR;
    int fIpos = 0, fOpos = 0, fCnt = 0, fCpos = 0, fP = 400;
    double fRph = 0.0, fNorm = 0.0, fMetal = 0.0, fRing = 0.0, fMix = 1.0;
};

} // namespace nullrobotizer
