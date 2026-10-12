/*
 * NULL JSFX - Spectral Freeze, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/spectral_freeze.jsfx, computed in double like EEL2 (FFTs with
 * EelFft.hpp, EEL2's fft/ifft/fft_permute semantics): 2048-point Hann analysis every 512
 * samples keeps the magnitudes fresh until Freeze is on; then each hop resynthesises a frame
 * from the held magnitudes with advancing phases (randomised by Smear with the JSFX's
 * Park-Miller generator) and overlap-adds it. Fade crossfades in and out of the frozen sound.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullfreeze {

static constexpr int kN = 2048, kH = kN / 4, kNB = kN / 2 + 1;

class FreezeProcessor {
public:
    FreezeProcessor()
        : fFft(2 * kN), fWin(kN), fInL(kN), fInR(kN), fMagL(kNB), fMagR(kNB), fPhL(kNB), fPhR(kNB), fOutL(kN),
          fOutR(kN)
    {
    }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (std::vector<double>* v : { &fInL, &fInR, &fMagL, &fMagR, &fPhL, &fPhR, &fOutL, &fOutR })
            std::fill(v->begin(), v->end(), 0.0);
        for (int i = 0; i < kN; ++i)
            fWin[size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kN);
        fSeed = 12345.0;
        for (int i = 0; i < kNB; ++i)
        {
            fPhL[size_t(i)] = rnd() * 2.0 * M_PI;
            fPhR[size_t(i)] = rnd() * 2.0 * M_PI;
        }
        fIpos = fOpos = fCnt = fFilled = 0;
        fFrozen = false;
        fWet = 0.0;
    }

    // @slider
    void setParams(bool freeze, double fadeMs, double smearPct, double levelDb, double mixPct)
    {
        fWant = freeze;
        fFade = 1.0 / std::max(1.0, fadeMs / 1000.0 * fSr);
        fSmear = smearPct / 100.0;
        fLevel = std::pow(10.0, levelDb / 20.0);
        fMix = mixPct / 100.0;
    }

    bool frozen() const noexcept { return fFrozen && fWant; }
    double wet() const noexcept { return fWet; }
    // the held magnitude of bin i, both channels averaged
    double magnitude(int i) const noexcept { return (fMagL[size_t(i)] + fMagR[size_t(i)]) / 2.0; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            fInL[size_t(fIpos)] = xl;
            fInR[size_t(fIpos)] = xr;
            if (++fIpos >= kN)
                fIpos = 0;
            if (fFilled < kN)
                ++fFilled;

            if (++fCnt >= kH)
            {
                fCnt = 0;
                if (!fFrozen)
                {
                    analyse(fInL, fMagL);
                    analyse(fInR, fMagR);
                }
                if (fWant && fFilled >= kN)
                    fFrozen = true;
                if (fWet > 0.0 || fFrozen)
                {
                    synth(fMagL, fPhL, fOutL);
                    synth(fMagR, fPhR, fOutR);
                }
            }
            if (!fWant && fWet <= 0.0)
                fFrozen = false;
            fWet = fFrozen && fWant ? std::min(1.0, fWet + fFade) : std::max(0.0, fWet - fFade);

            const double fl = fOutL[size_t(fOpos)], fr = fOutR[size_t(fOpos)];
            fOutL[size_t(fOpos)] = 0.0;
            fOutR[size_t(fOpos)] = 0.0;
            if (++fOpos >= kN)
                fOpos = 0;

            const double w = fWet * fMix;
            outL[n] = static_cast<T>(xl * (1.0 - w) + fl * fLevel * w);
            outR[n] = static_cast<T>(xr * (1.0 - w) + fr * fLevel * w);
        }
    }

private:
    // pm_rand: Park-Miller in exact doubles, 0 < value < 1
    double rnd() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    void analyse(const std::vector<double>& inb, std::vector<double>& magb)
    {
        double* b = fFft.data();
        for (int i = 0; i < kN; ++i)
        {
            b[2 * i] = inb[size_t((fIpos + i) % kN)] * fWin[size_t(i)];
            b[2 * i + 1] = 0.0;
        }
        eelfft::fft(b, kN);
        eelfft::fftPermute(b, kN);
        for (int i = 0; i < kNB; ++i)
        {
            const double re = b[2 * i], im = b[2 * i + 1];
            magb[size_t(i)] = std::sqrt(re * re + im * im);
        }
    }

    void synth(const std::vector<double>& magb, std::vector<double>& phb, std::vector<double>& outb)
    {
        double* b = fFft.data();
        for (int i = 0; i < kNB; ++i)
        {
            double p = phb[size_t(i)] + 2.0 * M_PI * i * kH / kN + fSmear * (rnd() * 2.0 - 1.0) * M_PI;
            p -= std::floor(p / (2.0 * M_PI)) * 2.0 * M_PI;
            phb[size_t(i)] = p;
            const double m = magb[size_t(i)];
            b[2 * i] = m * std::cos(p);
            b[2 * i + 1] = m * std::sin(p);
            if (i > 0 && i < kN / 2)
            {
                b[2 * (kN - i)] = m * std::cos(p);
                b[2 * (kN - i) + 1] = -m * std::sin(p);
            }
        }
        b[1] = 0.0;
        b[kN + 1] = 0.0;
        eelfft::fftIpermute(b, kN);
        eelfft::ifft(b, kN);
        for (int i = 0; i < kN; ++i)
            outb[size_t((fOpos + i) % kN)] += b[2 * i] * fWin[size_t(i)] / kN;
    }

    double fSr = 48000.0;
    std::vector<double> fFft, fWin, fInL, fInR, fMagL, fMagR, fPhL, fPhR, fOutL, fOutR;
    int fIpos = 0, fOpos = 0, fCnt = 0, fFilled = 0;
    bool fFrozen = false, fWant = false;
    double fWet = 0.0, fFade = 0.0, fSmear = 0.4, fLevel = 1.0, fMix = 1.0, fSeed = 12345.0;
};

} // namespace nullfreeze
