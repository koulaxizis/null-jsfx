/*
 * NULL JSFX - Spectral Blur, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/spectral_blur.jsfx, computed in double like EEL2 (FFTs with
 * EelFft.hpp, EEL2's fft/ifft/fft_permute semantics): every 512 samples a 2048-point Hann frame
 * of each channel is analysed, its bins are shifted by Shift, tilted, smoothed over time (Smear,
 * rising bins keep their phase, fading ones turn on with the JSFX's Park-Miller drift), gated
 * per bin and overlap-added. The latency is N - 1 = 2047 samples; the dry signal is delayed
 * to match.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullblur {

static constexpr int kN = 2048, kH = kN / 4, kNB = kN / 2 + 1;

class BlurProcessor {
public:
    BlurProcessor()
        : fFft(2 * kN), fWin(kN), fInL(kN), fInR(kN), fOutL(kN), fOutR(kN), fSmL(kNB), fSmR(kNB), fPhL(kNB),
          fPhR(kNB), fMi(kNB), fPi(kNB), fTg(kNB)
    {
    }

    static constexpr uint32_t latency() noexcept { return uint32_t(kN - 1); } // pdc_delay

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (std::vector<double>* v : { &fInL, &fInR, &fOutL, &fOutR, &fSmL, &fSmR, &fPhL, &fPhR, &fMi, &fPi, &fTg })
            std::fill(v->begin(), v->end(), 0.0);
        for (int i = 0; i < kN; ++i)
            fWin[size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kN);
        fSeed = 12345.0;
        fIp = fOp = fCnt = 0;
        fRq = 0.0;
    }

    // @slider, v = slider1..slider6 as the JSFX holds them
    void setParams(const double* v)
    {
        const double s = v[0] / 100.0;
        fSa = 1.0 - std::exp(-kH / std::max(1.0, 0.15 * s * s * fSr));
        fSc = 1.0 - std::exp(-kH / std::max(1.0, 3.0 * s * s * fSr));
        fSb = std::floor(v[1] * kN / fSr + 0.5);
        const double te = v[2] / (20.0 * std::log10(2.0));
        for (int i = 0; i < kNB; ++i)
            fTg[size_t(i)] = std::pow(std::min(10000.0, std::max(100.0, i * fSr / kN)) / 1000.0, te);
        fGl = v[3] <= -90.0 ? 0.0 : std::pow(10.0, v[3] / 20.0) * kN / 4.0;
        fMix = v[4] / 100.0;
        fOut = std::pow(10.0, v[5] / 20.0);
    }

    // the blurred magnitude of bin i, both channels averaged
    double magnitude(int i) const noexcept { return (fSmL[size_t(i)] + fSmR[size_t(i)]) / 2.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            fInL[size_t(fIp)] = inL[n];
            fInR[size_t(fIp)] = inR[n];
            if (++fIp >= kN)
                fIp = 0;
            const double dl = fInL[size_t(fIp)], dr = fInR[size_t(fIp)];
            if (++fCnt >= kH)
            {
                fCnt = 0;
                frame(fInL, fSmL, fPhL, fOutL);
                frame(fInR, fSmR, fPhR, fOutR);
                fRq += fSb;
                fRq -= std::floor(fRq / 4.0) * 4.0;
            }
            const double wl = fOutL[size_t(fOp)], wr = fOutR[size_t(fOp)];
            fOutL[size_t(fOp)] = 0.0;
            fOutR[size_t(fOp)] = 0.0;
            if (++fOp >= kN)
                fOp = 0;
            outL[n] = static_cast<T>((dl + (wl - dl) * fMix) * fOut);
            outR[n] = static_cast<T>((dr + (wr - dr) * fMix) * fOut);
        }
    }

private:
    // sp_rand: Park-Miller in exact doubles, 0 < value < 1
    double rnd() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    // sp_frame
    void frame(const std::vector<double>& inb, std::vector<double>& smb, std::vector<double>& phb,
               std::vector<double>& outb)
    {
        double* b = fFft.data();
        for (int i = 0; i < kN; ++i)
        {
            b[2 * i] = inb[size_t((fIp + i) % kN)] * fWin[size_t(i)];
            b[2 * i + 1] = 0.0;
        }
        eelfft::fft(b, kN);
        eelfft::fftPermute(b, kN);
        for (int i = 0; i < kNB; ++i)
        {
            const double re = b[2 * i], im = b[2 * i + 1];
            fMi[size_t(i)] = std::sqrt(re * re + im * im);
            fPi[size_t(i)] = std::atan2(im, re);
        }
        for (int i = 0; i < kNB; ++i)
        {
            const double j = i - fSb;
            double m, p;
            if (j >= 0.0 && j < kNB)
            {
                m = fMi[size_t(j)] * fTg[size_t(i)];
                p = fPi[size_t(j)] + fRq * 0.5 * M_PI;
            }
            else
            {
                m = 0.0;
                p = 0.0;
            }
            double s = smb[size_t(i)];
            s += (m - s) * (m > s ? fSa : fSc);
            smb[size_t(i)] = s;
            const double r = rnd() * 2.0 - 1.0;
            if (m < 0.5 * s)
                p = phb[size_t(i)] + 2.0 * M_PI * i * kH / kN + r * 0.5 * M_PI;
            p -= std::floor(p / (2.0 * M_PI)) * 2.0 * M_PI;
            phb[size_t(i)] = p;
            const double g = s < fGl ? s * (s / fGl) * (s / fGl) : s;
            b[2 * i] = g * std::cos(p);
            b[2 * i + 1] = g * std::sin(p);
            if (i > 0 && i < kN / 2)
            {
                b[2 * (kN - i)] = g * std::cos(p);
                b[2 * (kN - i) + 1] = -g * std::sin(p);
            }
        }
        b[1] = 0.0;
        b[kN + 1] = 0.0;
        eelfft::fftIpermute(b, kN);
        eelfft::ifft(b, kN);
        for (int i = 0; i < kN; ++i)
            outb[size_t((fOp + i) % kN)] += b[2 * i] * fWin[size_t(i)] / (kN * 1.5);
    }

    double fSr = 48000.0;
    std::vector<double> fFft, fWin, fInL, fInR, fOutL, fOutR, fSmL, fSmR, fPhL, fPhR, fMi, fPi, fTg;
    int fIp = 0, fOp = 0, fCnt = 0;
    double fRq = 0.0, fSa = 1.0, fSc = 1.0, fSb = 0.0, fGl = 0.0, fMix = 1.0, fOut = 1.0, fSeed = 12345.0;
};

} // namespace nullblur
