/*
 * NULL JSFX - Spectrum Analyzer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/spectrum_analyzer.jsfx in double: every hop (25 ms, at most
 * half a frame) the last N samples of the chosen channel get a Hann window and an FFT; each
 * bin's power (0 dB = a full-scale sine) rises at once and falls with Release, and a second
 * set holds the peaks (2 s or forever). The FFT here is a plain radix-2 one: the JSFX's EEL2
 * fft computes the same transform, and the bins only feed the display. The audio passes
 * through (Monitor Solo plays the analysed channel on both sides), exactly as in the JSFX.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nullspec {

static constexpr int kMaxN = 16384;
static constexpr int kRing = 16384;

// the bins the interface draws; written by the audio thread, read by the UI
struct View {
    float spec[kMaxN / 2 + 1] = {}, hold[kMaxN / 2 + 1] = {};
    std::atomic<int> n { 4096 };
    std::atomic<float> sampleRate { 48000.0f };
};

inline int fftSize(float v)
{
    const int i = int(std::lround(v));
    return i == 0 ? 1024 : i == 1 ? 2048 : i == 2 ? 4096 : i == 3 ? 8192 : 16384;
}

class SpectrumAnalyzer {
public:
    SpectrumAnalyzer() : fRing(kRing), fWin(kMaxN), fRe(kMaxN), fIm(kMaxN), fTwR(kMaxN / 2), fTwI(kMaxN / 2), fSpec(kMaxN / 2 + 1),
                         fHold(kMaxN / 2 + 1), fHoldT(kMaxN / 2 + 1) {}

    void setView(View* v) noexcept { fView = v; }

    // @init
    void setSampleRate(double sr) noexcept
    {
        fSr = sr;
        std::fill(fRing.begin(), fRing.end(), 0.0);
        std::fill(fSpec.begin(), fSpec.end(), 0.0);
        std::fill(fHold.begin(), fHold.end(), 0.0);
        std::fill(fHoldT.begin(), fHoldT.end(), 0.0);
        fWp = 0;
        fHc = 0;
        fN = 0;
        fHMode = -1;
        if (fView)
        {
            std::fill(std::begin(fView->spec), std::end(fView->spec), 0.0f);
            std::fill(std::begin(fView->hold), std::end(fView->hold), 0.0f);
            fView->sampleRate.store(float(sr), std::memory_order_relaxed);
        }
    }

    // @slider
    void setParams(int n, double releaseMs, int holdMode, int channel, bool solo) noexcept
    {
        if (n != fN || holdMode != fHMode)
        {
            if (n != fN)
            {
                fN = n;
                for (int i = 0; i < fN; ++i)
                    fWin[i] = 0.5 - 0.5 * std::cos(2.0 * 3.141592653589793 * i / fN);
                for (int k = 0; k < fN / 2; ++k)
                {
                    fTwR[k] = std::cos(-2.0 * 3.141592653589793 * k / fN);
                    fTwI[k] = std::sin(-2.0 * 3.141592653589793 * k / fN);
                }
                std::fill(fSpec.begin(), fSpec.end(), 0.0);
            }
            fHMode = holdMode;
            std::fill(fHold.begin(), fHold.end(), 0.0);
            std::fill(fHoldT.begin(), fHoldT.end(), 0.0);
            if (fView)
            {
                std::fill(std::begin(fView->spec), std::end(fView->spec), 0.0f);
                std::fill(std::begin(fView->hold), std::end(fView->hold), 0.0f);
                fView->n.store(fN, std::memory_order_relaxed);
            }
        }
        fHop = std::min(fN / 2.0, std::floor(fSr / 40.0 + 0.5));
        fNorm = 16.0 / (double(fN) * fN);
        fRc = releaseMs > 0.0 ? std::exp(-fHop / (fSr * releaseMs / 1000.0)) : 0.0;
        fHFrames = std::ceil(2.0 * fSr / fHop);
        fChan = channel;
        fSolo = solo;
    }

    // @sample for a block
    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            const double x = fChan == 0 ? (l + r) * 0.5 : fChan == 1 ? l : fChan == 2 ? r : (l - r) * 0.5;
            fRing[fWp] = x;
            fWp += 1;
            if (fWp >= kRing)
                fWp = 0;
            fHc += 1;
            if (fHc >= fHop)
            {
                fHc = 0;
                frame();
            }
            if (fSolo)
            {
                outL[i] = float(x);
                outR[i] = float(x);
            }
            else
            {
                outL[i] = inL[i];
                outR[i] = inR[i];
            }
        }
    }

private:
    // in-place iterative radix-2 FFT of fRe/fIm (length fN), natural order out
    void fft() noexcept
    {
        const int n = fN;
        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
            {
                std::swap(fRe[i], fRe[j]);
                std::swap(fIm[i], fIm[j]);
            }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int stride = n / len;
            for (int k = 0; k < len / 2; ++k)
            {
                const double wr = fTwR[k * stride], wi = fTwI[k * stride];
                for (int i = k; i < n; i += len)
                {
                    const int j = i + len / 2;
                    const double tr = fRe[j] * wr - fIm[j] * wi, ti = fRe[j] * wi + fIm[j] * wr;
                    fRe[j] = fRe[i] - tr;
                    fIm[j] = fIm[i] - ti;
                    fRe[i] += tr;
                    fIm[i] += ti;
                }
            }
        }
    }

    void frame() noexcept
    {
        int p = fWp - fN;
        if (p < 0)
            p += kRing;
        for (int i = 0; i < fN; ++i)
        {
            fRe[i] = fRing[p] * fWin[i];
            fIm[i] = 0.0;
            p += 1;
            if (p >= kRing)
                p = 0;
        }
        fft();
        for (int k = 0; k <= fN / 2; ++k)
        {
            const double pw = (fRe[k] * fRe[k] + fIm[k] * fIm[k]) * fNorm;
            double s = fSpec[k];
            s = pw > s ? pw : s * fRc + pw * (1.0 - fRc);
            fSpec[k] = s;
            if (fHMode > 0)
            {
                if (s >= fHold[k])
                {
                    fHold[k] = s;
                    fHoldT[k] = fHFrames;
                }
                else if (fHMode == 1)
                {
                    if (fHoldT[k] > 0.0)
                        fHoldT[k] -= 1.0;
                    else
                        fHold[k] = s;
                }
            }
            if (fView)
            {
                fView->spec[k] = float(s);
                fView->hold[k] = float(fHold[k]);
            }
        }
    }

    View* fView = nullptr;
    std::vector<double> fRing, fWin, fRe, fIm, fTwR, fTwI, fSpec, fHold, fHoldT;
    double fSr = 48000.0, fHop = 1200.0, fNorm = 0.0, fRc = 0.0, fHFrames = 80.0, fHc = 0.0;
    int fWp = 0, fN = 0, fHMode = -1, fChan = 0;
    bool fSolo = false;
};

} // namespace nullspec
