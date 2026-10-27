/*
 * NULL JSFX - Tuner, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/tuner.jsfx, computed in double in the EEL2 order: the input
 * low passed (two Butterworth sections) and taken down to 22 to 24 kHz, and every 512 of those
 * samples YIN on the last 1024 + up to 960 (FFT autocorrelation with EEL2's fft, cumulative
 * mean normalised difference, first dip below 0.15 or the deepest below 0.35, a parabola on
 * the difference function, refined at 2, 4, 8 ... periods). The frequency glides within 50
 * cents and holds half a second over silence. The audio passes through or is muted. The View
 * carries the frequency to the interface.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace nulltuner {

// what the interface draws
struct View {
    std::atomic<float> freq { 0.0f };   // Hz, 0 = no note
};

class Tuner {
public:
    // @init
    void setSampleRate(double sr)
    {
        (void)eelfft::detail::tables();   // the FFT tables, built here and not on the audio thread
        std::fill(fX, fX + 4096, 0.0);
        std::fill(fD, fD + 1100, 0.0);
        std::fill(fE, fE + 1100, 0.0);
        fDec = std::max(1.0, std::ceil(sr / 24000.0));
        fFd = sr / fDec;
        fTmax = std::min(1023.0, std::floor(fFd / 25.0));
        fTmin = std::max(2.0, std::floor(fFd / 4500.0));
        fN = 1024.0 + fTmax + 1.0;
        const double w = 2.0 * M_PI * std::min(0.42 * fFd, 0.45 * sr) / sr;
        const double al = std::sin(w) / (2.0 * std::sqrt(0.5));
        const double a0 = 1.0 + al;
        fB0 = (1.0 - std::cos(w)) / 2.0 / a0;
        fB1 = (1.0 - std::cos(w)) / a0;
        fA1 = -2.0 * std::cos(w) / a0;
        fA2 = (1.0 - al) / a0;
        fS1 = fS2 = fS3 = fS4 = 0.0;
        fDc = 0.0;
        fXw = 0;
        fHop = 0.0;
        fF = 0.0;
        fHold = 0.0;
        fHn = std::floor(0.5 * sr);
    }

    // @slider
    void setParams(double input, double output)
    {
        fIn = int(std::floor(input + 0.5));
        fMute = output >= 0.5;
    }

    double frequency() const noexcept { return fF; }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            double x = fIn == 1 ? l : fIn == 2 ? r : (l + r) * 0.5;
            double y = fB0 * x + fS1;
            fS1 = fB1 * x - fA1 * y + fS2;
            fS2 = fB0 * x - fA2 * y;
            x = y;
            y = fB0 * x + fS3;
            fS3 = fB1 * x - fA1 * y + fS4;
            fS4 = fB0 * x - fA2 * y;
            fDc += 1.0;
            if (fDc >= fDec)
            {
                fDc = 0.0;
                fX[fXw] = y;
                fXw = (fXw + 1) % 4096;
                fHop += 1.0;
                if (fHop >= 512.0)
                {
                    fHop = 0.0;
                    const double v = yin();
                    if (v > 0.0)
                    {
                        if (fF > 0.0 && std::fabs(1200.0 * std::log(v / fF) / std::log(2.0)) < 50.0)
                            fF *= std::pow(v / fF, 0.35);
                        else
                            fF = v;
                        fHold = fHn;
                    }
                    else
                    {
                        fHold -= 512.0 * fDec;
                        if (fHold <= 0.0)
                        {
                            fHold = 0.0;
                            fF = 0.0;
                        }
                    }
                }
            }
            outL[i] = fMute ? 0.0f : float(l);
            outR[i] = fMute ? 0.0f : float(r);
        }
    }

private:
    double x(double i) const { return fX[(int(i) % 4096 + 4096) % 4096]; }

    // the parabola's vertex offset around lag t of the difference function
    double vertex(int t) const
    {
        const double dl = fE[t - 1], dr = fE[t + 1];
        const double dd = dl - 2.0 * fE[t] + dr;
        return dd > 0.0 ? std::min(1.0, std::max(-1.0, (dl - dr) / (2.0 * dd))) : 0.0;
    }

    // tn_yin
    double yin()
    {
        std::fill(fA, fA + 8192, 0.0);
        std::fill(fB, fB + 8192, 0.0);
        const int n = int(fN), tmin = int(fTmin), tmax = int(fTmax);
        for (int i = 0; i < n; ++i)
        {
            const double j = x(fXw - fN + i + 4096);
            if (i < 1024)
                fA[i * 2] = j;
            fB[i * 2] = j;
        }
        double e0 = 0.0;
        for (int i = 0; i < 1024; ++i)
            e0 += fB[i * 2] * fB[i * 2];
        if (!(e0 / 1024.0 > 0.0000001))
            return 0.0;
        eelfft::fft(fA, 4096);
        eelfft::fft(fB, 4096);
        for (int i = 0; i < 4096; ++i)
        {
            const int k = i * 2;
            const double ar = fA[k], ai = fA[k + 1], br = fB[k], bi = fB[k + 1];
            fB[k] = ar * br + ai * bi;
            fB[k + 1] = ar * bi - ai * br;
        }
        eelfft::ifft(fB, 4096);
        double e = e0, s = 0.0;
        fD[0] = 1.0;
        for (int t = 1; t <= tmax + 1; ++t)
        {
            const double j = x(fXw - fN + t - 1 + 4096), k = x(fXw - fN + t - 1 + 1024 + 4096);
            e += k * k - j * j;
            const double d = std::max(0.0, e0 + e - 2.0 * fB[t * 2] / 4096.0);
            s += d;
            fE[t] = d;
            fD[t] = s > 0.0 ? d * t / s : 1.0;
        }
        int best = 0;
        for (int t = tmin; t <= tmax && !best; ++t)
            if (fD[t] < 0.15)
            {
                while (t < tmax && fD[t + 1] < fD[t])
                    ++t;
                best = t;
            }
        if (!best)
        {
            double bv = 0.35;
            for (int t = tmin; t <= tmax; ++t)
                if (fD[t] < bv)
                {
                    bv = fD[t];
                    best = t;
                }
        }
        if (!best)
            return 0.0;
        double p = best + vertex(best);
        for (double k = 2.0; k * p < fTmax - 1.0; k *= 2.0)
        {
            int t = int(std::floor(k * p + 0.5));
            while (t > 1 && fE[t - 1] < fE[t])
                --t;
            while (t < tmax && fE[t + 1] < fE[t])
                ++t;
            if (std::fabs(t - k * p) < std::max(1.0, p * 0.25))
                p = (t + vertex(t)) / k;
        }
        return fFd / p;
    }

    double fX[4096] = {}, fA[8192] = {}, fB[8192] = {}, fD[1100] = {}, fE[1100] = {};
    double fDec = 2.0, fFd = 24000.0, fTmax = 960.0, fTmin = 5.0, fN = 1985.0;
    double fB0 = 0.0, fB1 = 0.0, fA1 = 0.0, fA2 = 0.0, fS1 = 0.0, fS2 = 0.0, fS3 = 0.0, fS4 = 0.0;
    double fDc = 0.0, fHop = 0.0, fF = 0.0, fHold = 0.0, fHn = 22050.0;
    int fXw = 0, fIn = 0;
    bool fMute = false;
};

} // namespace nulltuner
