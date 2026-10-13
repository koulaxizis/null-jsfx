/*
 * NULL JSFX - Resonance Suppressor, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/resonance_suppressor.jsfx, computed in double like EEL2 (FFTs
 * with EelFft.hpp, EEL2's fft/ifft/fft_permute semantics): every 512 samples the last 2048 of
 * both channels go through one complex FFT (left real, right imaginary) with a sqrt-Hann
 * window. Each bin's level (the mean power of both channels, in dB) is compared with the
 * spectral envelope, a moving average of the levels over a log-frequency window that Sharpness
 * narrows; what sticks out by more than the Selectivity threshold is turned down (Soft: 2:1
 * with a 6 dB knee, easing into Depth; Hard: all of the excess, up to Depth), smoothed with
 * Attack/Release per frame and across neighbouring bins, inside the Low/High range. The frame
 * is resynthesised with the same window and overlap-added. Latency: 2047 samples.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullrsp {

static constexpr int kN = 2048, kH = kN / 4, kNB = kN / 2 + 1;

class ResonanceSuppressorProcessor {
public:
    ResonanceSuppressorProcessor()
        : fFft(2 * kN), fWin(kN), fInL(kN), fInR(kN), fOutL(kN), fOutR(kN), fLev(kNB), fCs(kNB + 1), fGr(kNB),
          fLo(kNB), fHi(kNB), fWgt(kNB)
    {
    }

    static constexpr int latency() noexcept { return kN - 1; }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (std::vector<double>* v : { &fInL, &fInR, &fOutL, &fOutR, &fCs, &fGr })
            std::fill(v->begin(), v->end(), 0.0);
        std::fill(fLev.begin(), fLev.end(), -120.0);
        for (int i = 0; i < kN; ++i)
            fWin[size_t(i)] = std::sin(M_PI * i / kN);
        fIpos = fOpos = fCnt = 0;
    }

    // @slider
    void setParams(double depthDb, double sharpPct, double selPct, double attackMs, double releaseMs, double lowHz,
                   double highHz, bool hard, bool delta, double mixPct)
    {
        fDepth = depthDb;
        fThr = 2 + selPct / 100 * 10;
        fAtt = 1 - std::exp(-kH / (fSr * attackMs / 1000));
        fRel = 1 - std::exp(-kH / (fSr * releaseMs / 1000));
        fHard = hard;
        fDelta = delta;
        fMix = mixPct / 100;
        // the envelope window: 2 octaves wide at Sharpness 0 down to 1/8 octave at 100
        const double r = std::pow(2.0, 2 * std::pow(2.0, -4 * sharpPct / 100) / 2);
        for (int k = 0; k < kNB; ++k)
        {
            fLo[size_t(k)] = int(std::max(0.0, std::min(double(k - 3), std::floor(k / r))));
            fHi[size_t(k)] = int(std::min(double(kNB - 1), std::max(double(k + 3), std::ceil(k * r))));
            // the range: full from Low to High, fading out over half an octave outside it
            const double f = k * fSr / kN;
            double w = 0;
            if (k > 0)
            {
                w = std::max(0.0, std::min(1.0, 1 + 2 * std::log(f / lowHz) / std::log(2.0)));
                w *= std::max(0.0, std::min(1.0, 1 - 2 * std::log(f / highHz) / std::log(2.0)));
            }
            fWgt[size_t(k)] = w;
        }
    }

    // display: the level of bin k in dB (0 dB = a full-scale sine) and its reduction in dB
    double level(int k) const noexcept { return fLev[size_t(k)]; }
    double reduction(int k) const noexcept { return fGr[size_t(k)]; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            fInL[size_t(fIpos)] = inL[n];
            fInR[size_t(fIpos)] = inR[n];
            if (++fIpos >= kN)
                fIpos = 0;
            if (++fCnt >= kH)
            {
                fCnt = 0;
                frame();
            }
            const double wl = fOutL[size_t(fOpos)], wr = fOutR[size_t(fOpos)];
            fOutL[size_t(fOpos)] = 0;
            fOutR[size_t(fOpos)] = 0;
            if (++fOpos >= kN)
                fOpos = 0;
            // the dry signal, delayed to line up: the oldest sample in the input ring
            const double dl = fInL[size_t(fIpos)], dr = fInR[size_t(fIpos)];
            double yl = dl * (1 - fMix) + wl * fMix;
            double yr = dr * (1 - fMix) + wr * fMix;
            if (fDelta)
            {
                yl = dl - yl;
                yr = dr - yr;
            }
            outL[n] = static_cast<T>(yl);
            outR[n] = static_cast<T>(yr);
        }
    }

private:
    void frame()
    {
        double* b = fFft.data();
        for (int i = 0; i < kN; ++i)
        {
            const int p = (fIpos + i) % kN;
            b[2 * i] = fInL[size_t(p)] * fWin[size_t(i)];
            b[2 * i + 1] = fInR[size_t(p)] * fWin[size_t(i)];
        }
        eelfft::fft(b, kN);
        eelfft::fftPermute(b, kN);
        const double nrm = M_PI * M_PI / (4.0 * kN * kN);
        for (int k = 0; k < kNB; ++k)
        {
            const int p = k == 0 ? 0 : kN - k;
            const double re = b[2 * k], im = b[2 * k + 1], re2 = b[2 * p], im2 = b[2 * p + 1];
            const double pw = (re * re + im * im + re2 * re2 + im2 * im2) * nrm;
            fLev[size_t(k)] = 10 * std::log10(pw + 1e-12);
        }
        fCs[0] = 0;
        for (int k = 0; k < kNB; ++k)
            fCs[size_t(k + 1)] = fCs[size_t(k)] + fLev[size_t(k)];
        for (int k = 0; k < kNB; ++k)
        {
            const int lo = fLo[size_t(k)], hi = fHi[size_t(k)];
            const double env = (fCs[size_t(hi + 1)] - fCs[size_t(lo)]) / (hi - lo + 1);
            const double x = fLev[size_t(k)] - env - fThr;
            double r = 0;
            if (x > 0 && fDepth > 0)
            {
                if (fHard)
                    r = std::min(fDepth, x);
                else
                {
                    r = x < 6 ? x * x / 24 : 0.5 * (x - 3);
                    r = fDepth * (1 - std::exp(-r / fDepth));
                }
            }
            r *= fWgt[size_t(k)];
            const double g = fGr[size_t(k)];
            fGr[size_t(k)] = g + (r > g ? fAtt : fRel) * (r - g);
        }
        for (int k = 0; k < kNB; ++k)
        {
            double g = 0.25 * fGr[size_t(std::max(0, k - 1))] + 0.5 * fGr[size_t(k)] + 0.25 * fGr[size_t(std::min(kNB - 1, k + 1))];
            g = std::pow(10.0, -g / 20);
            b[2 * k] *= g;
            b[2 * k + 1] *= g;
            if (k > 0 && k < kN / 2)
            {
                b[2 * (kN - k)] *= g;
                b[2 * (kN - k) + 1] *= g;
            }
        }
        eelfft::fftIpermute(b, kN);
        eelfft::ifft(b, kN);
        // sqrt-Hann twice at 4x overlap sums to 2; ifft() leaves the factor N
        const double sc = 1.0 / (2.0 * kN);
        for (int i = 0; i < kN; ++i)
        {
            const int p = (fOpos + i) % kN;
            fOutL[size_t(p)] += b[2 * i] * fWin[size_t(i)] * sc;
            fOutR[size_t(p)] += b[2 * i + 1] * fWin[size_t(i)] * sc;
        }
    }

    double fSr = 48000.0;
    std::vector<double> fFft, fWin, fInL, fInR, fOutL, fOutR, fLev, fCs, fGr;
    std::vector<int> fLo, fHi;
    std::vector<double> fWgt;
    int fIpos = 0, fOpos = 0, fCnt = 0;
    double fDepth = 6, fThr = 7, fAtt = 0.5, fRel = 0.1, fMix = 1;
    bool fHard = false, fDelta = false;
};

} // namespace nullrsp
