/*
 * NULL JSFX - the spectrum behind the curve of the EQ interfaces (Parametric EQ, Draw EQ, Studio EQ)
 * and the spectra of Reference
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Display only, like the GFX JSFX: the audio thread writes the mono input (pre) and output
 * (post) into a ring (Tap); the interface, at each repaint, takes the last 4096 samples of one
 * of them, a Hann window and an FFT (the radix-2 transform of Spectrum Analyzer), and smooths
 * each bin's power (0 dB = a full-scale sine): it rises at once and falls over 300 ms. bandDb()
 * gives a plot column the loudest bin under it (or the nearest two interpolated), tilted
 * 4.5 dB/oct around 1 kHz, as pe_spectrum / dg_spectrum in the GFX JSFX.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace nulleqan {

static constexpr int kN = 4096;

// written by the audio thread, read by the interface (direct access; a torn read only draws
// one odd frame)
struct Tap {
    float pre[kN] = {}, post[kN] = {};
    std::atomic<uint32_t> pos { 0 };
    std::atomic<float> sampleRate { 48000.0f };

    void push(float a, float b) noexcept
    {
        const uint32_t p = pos.load(std::memory_order_relaxed);
        pre[p] = a;
        post[p] = b;
        pos.store((p + 1) & uint32_t(kN - 1), std::memory_order_release);
    }
};

// the interface side: FFT, smoothing, columns
class Analyzer {
public:
    Analyzer()
    {
        for (int i = 0; i < kN; ++i)
            fWin[i] = 0.5 - 0.5 * std::cos(2.0 * 3.141592653589793 * i / kN);
        for (int k = 0; k < kN / 2; ++k)
        {
            fTwR[k] = std::cos(-2.0 * 3.141592653589793 * k / kN);
            fTwI[k] = std::sin(-2.0 * 3.141592653589793 * k / kN);
        }
    }

    // one frame from tap (post or pre) at time now (seconds); average = true averages each bin
    // over about a second instead (Reference)
    void update(const Tap& tap, bool post, double now, bool average = false)
    {
        const double dt = fLast > 0.0 ? std::max(0.0, std::min(0.5, now - fLast)) : 0.0;
        fLast = now;
        const double rc = std::exp(-dt / (average ? 1.0 : 0.3));
        fSr = tap.sampleRate.load(std::memory_order_relaxed);
        if (!(fSr > 0.0))
            fSr = 48000.0;
        uint32_t p = tap.pos.load(std::memory_order_acquire);
        const float* src = post ? tap.post : tap.pre;
        for (int i = 0; i < kN; ++i)
        {
            fRe[i] = double(src[p]) * fWin[i];
            fIm[i] = 0.0;
            p = (p + 1) & uint32_t(kN - 1);
        }
        fft();
        for (int k = 0; k <= kN / 2; ++k)
        {
            const double pw = (fRe[k] * fRe[k] + fIm[k] * fIm[k]) * 16.0 / (double(kN) * kN);
            const double s = fSp[k];
            fSp[k] = pw > s && !average ? pw : s * rc + pw * (1.0 - rc);
        }
    }

    // the dB of the column from f0 to f1 Hz; mean = true takes the mean of the bins under it
    // instead of the loudest (Reference)
    double bandDb(double f0, double f1, bool mean = false) const
    {
        const double k0 = f0 * kN / fSr, k1 = std::min(double(kN / 2), f1 * kN / fSr);
        int a = int(std::ceil(k0));
        const int b = int(std::floor(k1));
        double p = 0.0;
        if (a <= b)
        {
            const int n = b - a + 1;
            for (; a <= b; ++a)
                p = mean ? p + fSp[a] : std::max(p, fSp[a]);
            if (mean)
                p /= n;
        }
        else
        {
            const double kc = std::min(double(kN / 2 - 1), (k0 + k1) * 0.5);
            const int i = int(std::floor(kc));
            p = fSp[i] + (fSp[i + 1] - fSp[i]) * (kc - i);
        }
        return 10.0 * std::log10(p + 1e-20) + 4.5 * std::log2(std::sqrt(f0 * f1) / 1000.0);
    }

private:
    void fft()
    {
        const int n = kN;
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

    double fWin[kN], fTwR[kN / 2], fTwI[kN / 2], fRe[kN], fIm[kN], fSp[kN / 2 + 1] = {};
    double fSr = 48000.0, fLast = 0.0;
};

} // namespace nulleqan
