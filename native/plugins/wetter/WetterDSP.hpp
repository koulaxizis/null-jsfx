/*
 * NULL JSFX - Wetter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/wetter.jsfx (computed in double, like EEL2): a compact
 * Freeverb-style reverb, four damped feedback combs and two allpasses per channel fed with
 * the mono sum. Amount raises the wet level (up to 45%) and the room size together.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullwetter {

static constexpr int kCombL[4] = { 1116, 1188, 1277, 1356 };
static constexpr int kCombR[4] = { 1139, 1211, 1300, 1379 };
static constexpr int kApL[2] = { 556, 341 };
static constexpr int kApR[2] = { 579, 364 };

struct Params {
    double fb, damp, wet, dry;
};

// @slider
inline Params paramsFor(double slider) noexcept
{
    const double amount = slider / 100.0;
    return { 0.70 + amount * 0.18, 0.35, amount * 0.45 * 0.25, 1.0 - amount * 0.25 };
}

// reverb time (s) to -60 dB at DC: 60 dB over the per-loop loss of the average left comb
inline double rt60(double fb) noexcept
{
    const double loop = (1116.0 + 1188.0 + 1277.0 + 1356.0) / 4.0 / 44100.0;
    return fb > 0.0 ? 3.0 * loop / -std::log10(fb) : 0.0;
}

struct Comb {
    std::vector<double> b;
    int len = 1, p = 0;
    double f = 0;
    void init(int length, double sc, int size)
    {
        b.assign(size_t(size), 0.0);
        len = std::max(1, int(std::floor(length * sc)));
        p = 0;
        f = 0;
    }
    double tick(double x, double fb, double damp) noexcept
    {
        const double y = b[size_t(p)];
        f = y * (1.0 - damp) + f * damp;
        b[size_t(p)] = x + f * fb;
        p = (p + 1) % len;
        return y;
    }
};

struct Allpass {
    std::vector<double> b;
    int len = 1, p = 0;
    void init(int length, double sc, int size)
    {
        b.assign(size_t(size), 0.0);
        len = std::max(1, int(std::floor(length * sc)));
        p = 0;
    }
    double tick(double x) noexcept
    {
        const double v = b[size_t(p)];
        const double y = v - x;
        b[size_t(p)] = x + v * 0.5;
        p = (p + 1) % len;
        return y;
    }
};

class WetterProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        const int bs = int(std::ceil(sr * 0.05)) + 2;
        const double sc = sr / 44100.0;
        for (int i = 0; i < 4; ++i)
        {
            fCl[i].init(kCombL[i], sc, bs);
            fCr[i].init(kCombR[i], sc, bs);
        }
        fAl[0].init(kApL[0], sc, bs);
        fAl[1].init(kApL[1], sc, bs);
        fAr[0].init(kApR[0], sc, bs);
        fAr[1].init(kApR[1], sc, bs);
    }

    void setAmount(double v) noexcept { fP = paramsFor(v); }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const double fb = fP.fb, damp = fP.damp, wet = fP.wet, dry = fP.dry;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double s0 = inL[i], s1 = inR[i];
            const double in = (s0 + s1) * 0.5;
            double l = fCl[0].tick(in, fb, damp);
            l = l + fCl[1].tick(in, fb, damp);
            l = l + fCl[2].tick(in, fb, damp);
            l = l + fCl[3].tick(in, fb, damp);
            double r = fCr[0].tick(in, fb, damp);
            r = r + fCr[1].tick(in, fb, damp);
            r = r + fCr[2].tick(in, fb, damp);
            r = r + fCr[3].tick(in, fb, damp);
            l = fAl[1].tick(fAl[0].tick(l));
            r = fAr[1].tick(fAr[0].tick(r));
            outL[i] = static_cast<T>(s0 * dry + l * wet);
            outR[i] = static_cast<T>(s1 * dry + r * wet);
        }
    }

private:
    Params fP = paramsFor(50.0);
    Comb fCl[4], fCr[4];
    Allpass fAl[2], fAr[2];
};

} // namespace nullwetter
