/*
 * NULL JSFX - Spring Reverb, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/spring_reverb.jsfx, computed in double like EEL2: the
 * high-passed, pre-delayed mono sum drives two springs (left and right, different lengths),
 * each a chain of 40 dispersive first-order all-passes inside a damped feedback delay loop.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullspring {

static constexpr int kStages = 40;

// the all-pass coefficient for Tension (%)
inline double coefficient(double tension) noexcept { return 0.35 + tension / 100.0 * 0.35; }

// group delay (samples) of the 40-stage all-pass chain at angular frequency w (for the display)
inline double groupDelay(double a, double w) noexcept
{
    return kStages * (1.0 - a * a) / (1.0 - 2.0 * a * std::cos(w) + a * a);
}

class SpringReverbProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fMaxLine = int(std::ceil(sr * 0.08));
        fMaxPre = int(std::ceil(sr * 0.1)) + 2;
        fApL.assign(kStages, 0.0);
        fApR.assign(kStages, 0.0);
        fLineL.assign(size_t(fMaxLine), 0.0);
        fLineR.assign(size_t(fMaxLine), 0.0);
        fPre.assign(size_t(fMaxPre), 0.0);
        fW = fP = 0;
        fLpL = fLpR = 0.0;
        fHpX = fHpY = 0.0;
        fLenL = int(std::floor(sr * 0.0437));
        fLenR = int(std::floor(sr * 0.0531));
    }

    // @slider; s[0..5] = slider1..slider6
    void setParams(const double* s)
    {
        fA = coefficient(s[1]);
        fGL = std::pow(10.0, -3.0 * (double(fLenL) / fSr + kStages * 0.0001) / s[0]);
        fGR = std::pow(10.0, -3.0 * (double(fLenR) / fSr + kStages * 0.0001) / s[0]);
        fGL = std::min(fGL, 0.97);
        fGR = std::min(fGR, 0.97);
        fTone = std::exp(-2.0 * M_PI * s[2] / fSr);
        fDrip = s[3] / 100.0;
        fPreD = int(std::max(1.0, std::floor(s[4] * fSr / 1000.0)));
        fMix = s[5] / 100.0;
        fHpC = std::exp(-2.0 * M_PI * 120.0 / fSr);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double l = inL[n], r = inR[n];
            const double x = (l + r) * 0.5;
            fHpY = fHpC * (fHpY + x - fHpX);
            fHpX = x;
            fPre[size_t(fP)] = fHpY;
            int pr = fP - fPreD;
            if (pr < 0)
                pr += fMaxPre;
            const double xin = fPre[size_t(pr)];
            if (++fP >= fMaxPre)
                fP = 0;

            int rp = fW - fLenL;
            if (rp < 0)
                rp += fMaxLine;
            const double fbL = fLineL[size_t(rp)];
            rp = fW - fLenR;
            if (rp < 0)
                rp += fMaxLine;
            const double fbR = fLineR[size_t(rp)];

            const double sL = disperse(xin + fbL * fGL, fApL.data());
            const double sR = disperse(xin + fbR * fGR, fApR.data());
            fLpL = (1.0 - fTone) * sL + fTone * fLpL;
            fLpR = (1.0 - fTone) * sR + fTone * fLpR;
            const double oL = fLpL + (sL - fLpL) * fDrip * 0.7;
            const double oR = fLpR + (sR - fLpR) * fDrip * 0.7;
            fLineL[size_t(fW)] = fLpL;
            fLineR[size_t(fW)] = fLpR;
            if (++fW >= fMaxLine)
                fW = 0;

            outL[n] = static_cast<T>(l * (1.0 - fMix) + oL * fMix);
            outR[n] = static_cast<T>(r * (1.0 - fMix) + oR * fMix);
        }
    }

private:
    double disperse(double x, double* apb) const noexcept
    {
        for (int i = 0; i < kStages; ++i)
        {
            const double s = apb[i];
            const double y = s - fA * x;
            apb[i] = x + fA * y;
            x = y;
        }
        return x;
    }

    double fSr = 48000.0;
    std::vector<double> fApL, fApR, fLineL, fLineR, fPre;
    int fMaxLine = 1, fMaxPre = 1, fW = 0, fP = 0, fLenL = 1, fLenR = 1, fPreD = 1;
    double fLpL = 0.0, fLpR = 0.0, fHpX = 0.0, fHpY = 0.0;
    double fA = 0.56, fGL = 0.0, fGR = 0.0, fTone = 0.0, fDrip = 0.0, fMix = 0.0, fHpC = 0.0;
};

} // namespace nullspring
