/*
 * NULL JSFX - Key Detect, framework-independent core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/key_detect.jsfx, computed in double in the EEL2 order (EEL2's
 * FFT through EelFft.hpp): every 4096 samples a Hann frame of the last 16384 (left and right
 * averaged); the bins from 65 Hz to 2.1 kHz add their magnitude, weighted by their distance to
 * the nearest semitone, to it; each semitone loses a third of the one 19 below and a fifth of
 * the one 28 below (its 3rd and 5th harmonics), they fold into pitch classes; the frame is scaled to a peak of 1 and added to a
 * running chroma that forgets with Window; frames quieter than Threshold (RMS) are skipped.
 * The chroma is correlated with the 24 key profiles (Krumhansl-Kessler or Temperley): the
 * best is the key, the runner-up is kept too. Hold keeps the result. The audio passes through.
 * The buffers are members (sized for N = 16384), so nothing is allocated while it plays.
 */

#pragma once

#include "EelFft.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullkeydetect {

struct Params {
    double window = 10.0, threshold = -50.0;
    int profile = 0, hold = 0;
};

class KeyDetect {
public:
    static constexpr int kN = 16384, kH = 4096;

    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        for (int i = 0; i < kN; ++i)
            fWin[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kN);
        static const double kProf[48] = {
            6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88, // Krumhansl major
            6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17, // Krumhansl minor
            5, 2, 3.5, 2, 4.5, 4, 2, 4.5, 2, 3.5, 1.5, 4,                           // Temperley major
            5, 2, 3.5, 4.5, 2, 4, 2, 4.5, 3.5, 2, 1.5, 4,                           // Temperley minor
        };
        std::memcpy(fProf, kProf, sizeof(fProf));
        for (int k = 0; k < kN / 2; ++k)
        {
            const double f = k * fSr / kN;
            if (f >= 65.0 && f <= 2100.0)
            {
                const double p = 12.0 * std::log(f / 440.0) / std::log(2.0) + 69.0;
                const double n = std::floor(p + 0.5);
                fPc[k] = int(n) - 36;
                fWt[k] = std::fmax(0.0, 1.0 - 2.0 * std::fabs(p - n));
            }
            else
            {
                fPc[k] = -1;
                fWt[k] = 0.0;
            }
        }
        reset();
        setParams(fP);
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fDec = std::exp(-kH / (p.window * fSr));
        fThr = std::pow(10.0, p.threshold / 20.0);
        fProfO = p.profile * 24;
    }

    void reset() noexcept
    {
        std::memset(fIn, 0, sizeof(fIn));
        std::memset(fC, 0, sizeof(fC));
        std::memset(fR, 0, sizeof(fR));
        fPos = fCnt = 0;
        fKey = fKey2 = -1;
        fConf = fConf2 = 0.0;
        fFrames = 0;
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            fIn[fPos] = (l + r) * 0.5;
            if (++fPos >= kN)
                fPos = 0;
            if (++fCnt >= kH)
            {
                fCnt = 0;
                if (!fP.hold)
                    frame();
            }
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

    // for the display
    double chroma(int i) const noexcept { return fC[i]; }
    int key() const noexcept { return fKey; }   // 0..11 major, 12..23 minor, -1 none yet
    int key2() const noexcept { return fKey2; }
    double confidence() const noexcept { return fConf; }
    double confidence2() const noexcept { return fConf2; }

private:
    void frame() noexcept
    {
        double e = 0.0;
        for (int i = 0; i < kN; ++i)
        {
            const double m = fIn[(fPos + i) % kN];
            e += m * m;
            fF[2 * i] = m * fWin[i];
            fF[2 * i + 1] = 0.0;
        }
        if (!(std::sqrt(e / kN) > fThr))
            return;
        eelfft::fft(fF, kN);
        eelfft::fftPermute(fF, kN);
        double fr[12] = {}, sm[61] = {};
        for (int k = 0; k < kN / 2; ++k)
            if (fPc[k] >= 0)
            {
                const double re = fF[2 * k], im = fF[2 * k + 1];
                sm[fPc[k]] += std::sqrt(re * re + im * im) * fWt[k];
            }
        for (int i = 0; i < 61; ++i)
        {
            const double m = sm[i] - (i >= 19 ? sm[i - 19] / 3.0 : 0.0) - (i >= 28 ? sm[i - 28] / 5.0 : 0.0);
            fr[i % 12] += std::fmax(0.0, m);
        }
        double mx = 0.0;
        for (int i = 0; i < 12; ++i)
            mx = std::fmax(mx, fr[i]);
        if (mx > 0.0)
        {
            for (int i = 0; i < 12; ++i)
                fC[i] = fC[i] * fDec + fr[i] / mx;
            ++fFrames;
        }
        double mean = 0.0;
        for (int i = 0; i < 12; ++i)
            mean += fC[i];
        mean /= 12.0;
        for (int md = 0; md < 2; ++md)
        {
            const double* pr = fProf + fProfO + md * 12;
            double pm = 0.0;
            for (int i = 0; i < 12; ++i)
                pm += pr[i];
            pm /= 12.0;
            for (int t = 0; t < 12; ++t)
            {
                double sxy = 0.0, sxx = 0.0, syy = 0.0;
                for (int i = 0; i < 12; ++i)
                {
                    const double sx = fC[i] - mean, sp = pr[(i - t + 12) % 12] - pm;
                    sxy += sx * sp;
                    sxx += sx * sx;
                    syy += sp * sp;
                }
                fR[md * 12 + t] = sxx > 0.0 && syy > 0.0 ? sxy / std::sqrt(sxx * syy) : 0.0;
            }
        }
        if (fFrames > 0)
        {
            fKey = fKey2 = -1;
            fConf = fConf2 = -2.0;
            for (int j = 0; j < 24; ++j)
            {
                if (fR[j] > fConf)
                {
                    fKey2 = fKey;
                    fConf2 = fConf;
                    fKey = j;
                    fConf = fR[j];
                }
                else if (fR[j] > fConf2)
                {
                    fKey2 = j;
                    fConf2 = fR[j];
                }
            }
        }
    }

    double fSr = 48000.0;
    Params fP;
    double fDec = 0.0, fThr = 0.0;
    int fProfO = 0;
    double fF[2 * kN] = {}, fIn[kN] = {}, fWin[kN] = {}, fWt[kN / 2] = {};
    int fPc[kN / 2] = {};
    double fC[12] = {}, fR[24] = {}, fProf[48] = {};
    int fPos = 0, fCnt = 0, fKey = -1, fKey2 = -1, fFrames = 0;
    double fConf = 0.0, fConf2 = 0.0;
};

// key k (0..11 major, 12..23 minor) as "A MINOR"
inline const char* keyNote(int k)
{
    static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k >= 0 ? kNames[k % 12] : "-";
}

} // namespace nullkeydetect
