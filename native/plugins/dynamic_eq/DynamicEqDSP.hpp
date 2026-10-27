/*
 * NULL JSFX - Dynamic EQ, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/dynamic_eq.jsfx, computed in double like EEL2: four bell bands
 * in series that only act when their band gets loud. Each band measures its own constant-peak
 * band-pass; once that level passes Threshold the band's Gain is applied, fully at 12 dB over.
 * Appended in 2.7: Channels (Mid / Side: only that half of the mid/side pair is equalised, on
 * both channels, the other half is added back after) and Listen Delta (the input minus the
 * result, before Makeup).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulldeq {

static constexpr int kBands = 4;

struct Band {
    double b0 = 0, a1 = 0, a2 = 0, gain = 0, thresh = -12;

    // the JSFX's setup(): constant-peak band-pass (RBJ)
    void setup(double freq, double q, double g, double th, double sr) noexcept
    {
        const double w = 2.0 * M_PI * std::min(freq, sr * 0.45) / sr;
        const double al = std::sin(w) / (2.0 * q);
        b0 = al / (1.0 + al);
        a1 = -2.0 * std::cos(w) / (1.0 + al);
        a2 = (1.0 - al) / (1.0 + al);
        gain = g;
        thresh = th;
    }

    // how far the gain is engaged (0..1) at envelope env (the GFX's de_over)
    double over(double env) const noexcept
    {
        const double o = env > 0.00001 ? (20.0 * std::log10(env) - thresh) / 12.0 : 0.0;
        return std::max(0.0, std::min(1.0, o));
    }

    // magnitude in dB of x + (A - 1) x band-pass at w, A from over (the GFX's de_db)
    double magnitudeDb(double w, double ov) const noexcept
    {
        const double A = std::pow(10.0, gain * ov / 20.0);
        const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
        const double nr = b0 * (1 - c2), ni = b0 * s2;
        const double dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        const double d2 = std::max(dr * dr + di * di, 1e-20);
        const double hr = 1 + (A - 1) * (nr * dr + ni * di) / d2;
        const double hi = (A - 1) * (ni * dr - nr * di) / d2;
        return 10.0 * std::log10(std::max(hr * hr + hi * hi, 1e-20));
    }
};

struct Params {
    double freq[kBands] = { 200, 1000, 2000, 6000 };
    double gain[kBands] = {}, q[kBands] = { 1, 1, 1, 1 }, thresh[kBands] = { -12, -12, -12, -12 };
    double attackMs = 1, releaseMs = 100, makeupDb = 0;
    int channels = 0; // 0 stereo, 1 mid, 2 side
    bool delta = false;
};

inline void setupBands(const Params& p, double sr, Band* bands) noexcept
{
    for (int i = 0; i < kBands; ++i)
        bands[i].setup(p.freq[i], p.q[i], p.gain[i], p.thresh[i], sr);
}

class DynamicEqProcessor {
public:
    void setSampleRate(double sr) { fSr = sr; apply(); }

    void reset()
    {
        for (auto& s : fS)
            s = State();
    }

    void setParams(const Params& p)
    {
        fP = p;
        apply();
    }

    // the envelope of band i (for the display)
    double envelope(int i) const noexcept { return fS[i].env; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            double s0 = inL[n], s1 = inR[n], other = 0.0;
            if (fP.channels != 0)
            {
                const double m = (s0 + s1) * 0.5;
                const double sd = (s0 - s1) * 0.5;
                if (fP.channels == 1)
                {
                    s0 = s1 = m;
                    other = sd;
                }
                else
                {
                    s0 = s1 = sd;
                    other = m;
                }
            }
            double l = s0, r = s1;
            for (int i = 0; i < kBands; ++i)
            {
                const Band& b = fB[i];
                State& s = fS[i];
                const double bl = b.b0 * (l - s.xl2) - b.a1 * s.yl1 - b.a2 * s.yl2;
                s.xl2 = s.xl1; s.xl1 = l; s.yl2 = s.yl1; s.yl1 = bl;
                const double br = b.b0 * (r - s.xr2) - b.a1 * s.yr1 - b.a2 * s.yr2;
                s.xr2 = s.xr1; s.xr1 = r; s.yr2 = s.yr1; s.yr1 = br;
                const double lev = std::max(std::fabs(bl), std::fabs(br));
                s.env = lev > s.env ? fAtt * s.env + (1 - fAtt) * lev : fRel * s.env + (1 - fRel) * lev;
                const double A = std::pow(10.0, b.gain * b.over(s.env) / 20.0);
                l += (A - 1) * bl;
                r += (A - 1) * br;
            }
            if (fP.delta)
            {
                l = s0 - l;
                r = s1 - r;
            }
            else
            {
                l = l * fMakeup;
                r = r * fMakeup;
            }
            if (fP.channels != 0)
            {
                // back to left / right (Delta: the processed half alone)
                if (fP.delta)
                    other = 0.0;
                const double x = l;
                if (fP.channels == 1)
                {
                    l = x + other;
                    r = x - other;
                }
                else
                {
                    l = other + x;
                    r = other - x;
                }
            }
            outL[n] = static_cast<T>(l);
            outR[n] = static_cast<T>(r);
        }
    }

private:
    struct State {
        double xl1 = 0, xl2 = 0, yl1 = 0, yl2 = 0, xr1 = 0, xr2 = 0, yr1 = 0, yr2 = 0, env = 0;
    };

    void apply()
    {
        setupBands(fP, fSr, fB);
        fAtt = std::exp(-1.0 / (fP.attackMs * fSr / 1000.0));
        fRel = std::exp(-1.0 / (fP.releaseMs * fSr / 1000.0));
        fMakeup = std::pow(10.0, fP.makeupDb / 20.0);
    }

    double fSr = 48000.0, fAtt = 0, fRel = 0, fMakeup = 1;
    Params fP;
    Band fB[kBands];
    State fS[kBands];
};

} // namespace nulldeq
