/*
 * NULL JSFX - Formant Filter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/formant_filter.jsfx, computed in double like EEL2: three RBJ
 * band-passes (constant 0 dB peak) at the formants of the selected vowel, summed, times 3 and
 * Gain. The formant frequencies glide (~15 ms, updated once per block like the JSFX's @block)
 * to a new vowel; the filter state is never reset.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullformant {

static constexpr double kPI = 3.14159265358979; // the JSFX's PI literal (not full precision)

// the formants F1..F3 of each vowel A, E, I, O, U
inline void vowelFormants(int vowel, double& t1, double& t2, double& t3) noexcept
{
    static const double kF[5][3] = {
        { 730, 1090, 2440 }, { 400, 1220, 2550 }, { 270, 2290, 3010 }, { 530, 1840, 2600 }, { 530, 1220, 2440 },
    };
    vowel = std::max(0, std::min(4, vowel));
    t1 = kF[vowel][0];
    t2 = kF[vowel][1];
    t3 = kF[vowel][2];
}

struct BandPass {
    double b0 = 0, b2 = 0, a1 = 0, a2 = 0;
    // bp_set
    void set(double f, double q, double sr) noexcept
    {
        const double w0 = 2 * kPI * (f / sr);
        const double alpha = std::sin(w0) / (2 * q);
        const double a0inv = 1 / (1 + alpha);
        b0 = a0inv * alpha;
        b2 = -a0inv * alpha;
        a1 = -2 * a0inv * std::cos(w0);
        a2 = a0inv * (1 - alpha);
    }
};

// display: the response in dB at f of the three band-passes for vowel, times 3 and Gain (ff_db)
inline double responseDb(double f, int vowel, double q, double gainDb, double sr) noexcept
{
    double t[3];
    vowelFormants(vowel, t[0], t[1], t[2]);
    const double g = std::exp(gainDb * 0.11512925464970228420);
    const double w = 2 * kPI * std::min(f, sr * 0.499) / sr;
    double re = 0, im = 0;
    for (double fc : t)
    {
        BandPass c;
        c.set(fc, std::max(1.0, q), sr);
        const double nr = c.b0 * (1 - std::cos(2 * w)), ni = c.b0 * std::sin(2 * w);
        const double dr = 1 + c.a1 * std::cos(w) + c.a2 * std::cos(2 * w);
        const double di = -(c.a1 * std::sin(w) + c.a2 * std::sin(2 * w));
        const double dd = dr * dr + di * di;
        re += (nr * dr + ni * di) / dd;
        im += (ni * dr - nr * di) / dd;
    }
    return 10 * std::log10(std::max((re * re + im * im) * 9 * g * g, 1e-10));
}

class FormantProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fFirst = true;
        for (State& s : fS)
            s = State();
    }

    // @slider
    void setParams(int vowel, double gainDb, double q)
    {
        fG = std::exp(gainDb * 0.11512925464970228420);
        fQ = std::max(1.0, q);
        vowelFormants(vowel, fT1, fT2, fT3);
        if (fFirst)
        {
            fF1 = fT1; fF2 = fT2; fF3 = fT3;
            fFirst = false;
        }
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        // @block
        const double k = 1 - std::exp(-double(frames) / (0.015 * fSr));
        fF1 += (fT1 - fF1) * k;
        fF2 += (fT2 - fF2) * k;
        fF3 += (fT3 - fF3) * k;
        fC[0].set(fF1, fQ, fSr);
        fC[1].set(fF2, fQ, fSr);
        fC[2].set(fF3, fQ, fSr);

        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            double sl = 0, sr = 0;
            for (int f = 0; f < 3; ++f)
            {
                sl = f == 0 ? tick(fC[f], fS[f * 2], xl) : sl + tick(fC[f], fS[f * 2], xl);
                sr = f == 0 ? tick(fC[f], fS[f * 2 + 1], xr) : sr + tick(fC[f], fS[f * 2 + 1], xr);
            }
            outL[n] = static_cast<T>(sl * fG * 3);
            outR[n] = static_cast<T>(sr * fG * 3);
        }
    }

private:
    struct State { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };

    static double tick(const BandPass& c, State& s, double x) noexcept
    {
        const double b1 = 0;
        // grouped the way EEL2 evaluates "b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2" (bit-exact)
        const double y = (c.b0 * x + b1 * s.x1) + ((c.b2 * s.x2 - c.a1 * s.y1) - c.a2 * s.y2);
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    double fSr = 48000.0, fG = 1.0, fQ = 10.0;
    double fT1 = 730, fT2 = 1090, fT3 = 2440, fF1 = 730, fF2 = 1090, fF3 = 2440;
    bool fFirst = true;
    BandPass fC[3];
    State fS[6];
};

} // namespace nullformant
