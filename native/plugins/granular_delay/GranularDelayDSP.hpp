/*
 * NULL JSFX - Granular Delay, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/granular_delay.jsfx, computed in double like EEL2: a 3 s
 * stereo delay line read by up to 16 Hann-windowed grains (size, density, pitch, spray,
 * reverse, random pan), the grain cloud fed back into the line. The random numbers come from
 * the same exact Park-Miller generator as the JSFX, so both render the same grains.
 * The memory layout (left line, right line, grain table) is the JSFX's, so even an edge read
 * one past a line lands where the JSFX's does.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullgranular {

class GranularDelayProcessor {
public:
    static constexpr int kGrains = 16;

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 3.0));
        fMem.assign(size_t(2 * fSize + 6 * kGrains + 8), 0.0);
        fW = 0;
        fNext = 0.0;
        fFbL = fFbR = 0.0;
        fSeed = 12345.0;
    }

    // @slider; s[0..8] = slider1..slider9
    void setParams(const double* s)
    {
        fDtime = s[0] * fSr / 1000.0;
        fGlen = std::max(32.0, std::floor(s[1] * fSr / 1000.0));
        fInterval = fSr / s[2];
        fRatio = std::pow(2.0, s[3] / 12.0);
        fSpray = s[4] / 100.0;
        fRev = s[5];
        fFeedback = s[6] / 100.0;
        fSpread = s[7] / 100.0;
        fMix = s[8] / 100.0;
        fGnorm = 1.0 / std::sqrt(std::max(1.0, fGlen / fInterval));
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        double* const bl = fMem.data();
        double* const br = bl + fSize;
        double* const gr = br + fSize;
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            bl[fW] = xl + fFbL * fFeedback;
            br[fW] = xr + fFbR * fFeedback;

            // start a new grain in the first free slot
            fNext -= 1.0;
            if (fNext <= 0.0)
            {
                fNext = fInterval * (0.75 + 0.5 * rnd());
                for (int i = 0; i < kGrains; ++i)
                {
                    double* g = gr + 6 * i;
                    if (g[0] <= 0.0)
                    {
                        const double dir = 100.0 * rnd() < fRev ? -1.0 : 1.0;
                        const double r = rnd();
                        g[1] = double(fW) - fDtime - fSpray * fSr * r - (dir < 0.0 ? 0.0 : fGlen * std::max(0.0, fRatio - 1.0));
                        g[2] = fRatio * dir;
                        g[0] = fGlen;
                        const double pan = 0.5 + (rnd() - 0.5) * fSpread;
                        g[3] = std::cos(pan * M_PI / 2.0);
                        g[4] = std::sin(pan * M_PI / 2.0);
                        if (dir < 0.0)
                            g[1] += fGlen * fRatio;
                        break;
                    }
                }
            }

            double ol = 0.0, orr = 0.0;
            for (int i = 0; i < kGrains; ++i)
            {
                double* g = gr + 6 * i;
                if (g[0] > 0.0)
                {
                    const double ph = 1.0 - g[0] / fGlen;
                    const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * ph);
                    ol += rd(bl, g[1]) * w * g[3];
                    orr += rd(br, g[1]) * w * g[4];
                    g[1] += g[2];
                    g[0] -= 1.0;
                }
            }
            ol *= fGnorm * 1.4;
            orr *= fGnorm * 1.4;
            fFbL = ol;
            fFbR = orr;

            if (++fW >= fSize)
                fW = 0;
            outL[n] = static_cast<T>(xl * (1.0 - fMix) + ol * fMix);
            outR[n] = static_cast<T>(xr * (1.0 - fMix) + orr * fMix);
        }
    }

private:
    // Park-Miller in exact doubles (the JSFX's rnd()), 0 < value < 1
    double rnd() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    // the JSFX's rd(): linear interpolation, position wrapped into the line
    double rd(const double* buf, double p) const noexcept
    {
        const double sz = double(fSize);
        p = p - std::floor(p / sz) * sz;
        const double i = std::floor(p), f = p - i;
        double i2 = i + 1.0;
        if (i2 >= sz)
            i2 = 0.0;
        return buf[size_t(i)] + (buf[size_t(i2)] - buf[size_t(i)]) * f;
    }

    double fSr = 48000.0;
    std::vector<double> fMem;
    int fSize = 1, fW = 0;
    double fNext = 0.0, fFbL = 0.0, fFbR = 0.0, fSeed = 12345.0;
    double fDtime = 0.0, fGlen = 32.0, fInterval = 1.0, fRatio = 1.0, fSpray = 0.0, fRev = 0.0;
    double fFeedback = 0.0, fSpread = 0.0, fMix = 0.0, fGnorm = 1.0;
};

} // namespace nullgranular
