/*
 * NULL JSFX - Parametric EQ, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/parametric_eq.jsfx, computed in double like EEL2: five RBJ
 * biquad bands in series, each any of eight shapes. Mode Bypass skips a band; Solo turns the
 * band into a band-pass and plays only the soloed bands.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullpeq {

static constexpr int kBands = 5;

struct Coefs {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

// the JSFX's design(): RBJ cookbook, type 0..7 = low shelf, high shelf, peaking, low pass,
// high pass, notch, band pass (0 dB peak), all pass
inline Coefs design(double freq, double gain, double q, int type, double sr) noexcept
{
    const double w = 2.0 * M_PI * std::min(freq, sr * 0.49) / sr;
    const double c = std::cos(w), s = std::sin(w);
    const double al = s / (2.0 * std::max(q, 0.05));
    const double A = std::pow(10.0, gain / 40.0), sq = std::sqrt(A);
    Coefs k;
    double a0;
    switch (type)
    {
    case 0:
        k.b0 = A * ((A + 1) - (A - 1) * c + 2 * sq * al);
        k.b1 = 2 * A * ((A - 1) - (A + 1) * c);
        k.b2 = A * ((A + 1) - (A - 1) * c - 2 * sq * al);
        a0 = (A + 1) + (A - 1) * c + 2 * sq * al;
        k.a1 = -2 * ((A - 1) + (A + 1) * c);
        k.a2 = (A + 1) + (A - 1) * c - 2 * sq * al;
        break;
    case 1:
        k.b0 = A * ((A + 1) + (A - 1) * c + 2 * sq * al);
        k.b1 = -2 * A * ((A - 1) + (A + 1) * c);
        k.b2 = A * ((A + 1) + (A - 1) * c - 2 * sq * al);
        a0 = (A + 1) - (A - 1) * c + 2 * sq * al;
        k.a1 = 2 * ((A - 1) - (A + 1) * c);
        k.a2 = (A + 1) - (A - 1) * c - 2 * sq * al;
        break;
    case 2:
        k.b0 = 1 + al * A; k.b1 = -2 * c; k.b2 = 1 - al * A;
        a0 = 1 + al / A; k.a1 = -2 * c; k.a2 = 1 - al / A;
        break;
    case 3:
        k.b0 = (1 - c) / 2; k.b1 = 1 - c; k.b2 = (1 - c) / 2;
        a0 = 1 + al; k.a1 = -2 * c; k.a2 = 1 - al;
        break;
    case 4:
        k.b0 = (1 + c) / 2; k.b1 = -(1 + c); k.b2 = (1 + c) / 2;
        a0 = 1 + al; k.a1 = -2 * c; k.a2 = 1 - al;
        break;
    case 5:
        k.b0 = 1; k.b1 = -2 * c; k.b2 = 1;
        a0 = 1 + al; k.a1 = -2 * c; k.a2 = 1 - al;
        break;
    case 6:
        k.b0 = al; k.b1 = 0; k.b2 = -al;
        a0 = 1 + al; k.a1 = -2 * c; k.a2 = 1 - al;
        break;
    default:
        k.b0 = 1 - al; k.b1 = -2 * c; k.b2 = 1 + al;
        a0 = 1 + al; k.a1 = -2 * c; k.a2 = 1 - al;
        break;
    }
    k.b0 /= a0; k.b1 /= a0; k.b2 /= a0; k.a1 /= a0; k.a2 /= a0;
    return k;
}

// magnitude in dB of a biquad at w radians per sample (the GFX's pe_db)
inline double magnitudeDb(const Coefs& k, double w) noexcept
{
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
    const double nr = k.b0 + k.b1 * c1 + k.b2 * c2, ni = -(k.b1 * s1 + k.b2 * s2);
    const double dr = 1 + k.a1 * c1 + k.a2 * c2, di = -(k.a1 * s1 + k.a2 * s2);
    return 10.0 * std::log10(std::max(nr * nr + ni * ni, 1e-20) / std::max(dr * dr + di * di, 1e-20));
}

struct BandParams {
    double freq = 1000, gain = 0, q = 1;
    int type = 2, mode = 0; // mode 0 active, 1 bypass, 2 solo
};

// the coefficients and the active flags of all bands (the JSFX's @slider)
struct EqSetup {
    Coefs k[kBands];
    bool active[kBands] = {};

    void compute(const BandParams* p, double sr) noexcept
    {
        bool soloAny = false;
        for (int i = 0; i < kBands; ++i)
            soloAny = soloAny || p[i].mode == 2;
        for (int i = 0; i < kBands; ++i)
        {
            k[i] = design(p[i].freq, p[i].gain, p[i].q, p[i].mode == 2 ? 6 : p[i].type, sr);
            active[i] = soloAny ? p[i].mode == 2 : p[i].mode != 1;
        }
    }

    // the whole EQ at f Hz in dB (the GFX's pe_total)
    double responseDb(double f, double sr) const noexcept
    {
        const double w = 2.0 * M_PI * std::min(f, sr * 0.4999) / sr;
        double db = 0.0;
        for (int i = 0; i < kBands; ++i)
            if (active[i])
                db += magnitudeDb(k[i], w);
        return db;
    }
};

class ParametricEqProcessor {
public:
    void setSampleRate(double sr) { fSr = sr; apply(); }
    void reset() { std::fill(&fZ[0][0], &fZ[0][0] + kBands * 8, 0.0); }

    void setParams(const BandParams* p)
    {
        std::copy(p, p + kBands, fP);
        apply();
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            double l = inL[n], r = inR[n];
            for (int i = 0; i < kBands; ++i)
            {
                if (!fS.active[i])
                    continue;
                const Coefs& k = fS.k[i];
                double* z = fZ[i]; // xl1 xl2 yl1 yl2 xr1 xr2 yr1 yr2
                double y = k.b0 * l + k.b1 * z[0] + k.b2 * z[1] - k.a1 * z[2] - k.a2 * z[3];
                z[1] = z[0]; z[0] = l; z[3] = z[2]; z[2] = y;
                l = y;
                y = k.b0 * r + k.b1 * z[4] + k.b2 * z[5] - k.a1 * z[6] - k.a2 * z[7];
                z[5] = z[4]; z[4] = r; z[7] = z[6]; z[6] = y;
                r = y;
            }
            outL[n] = static_cast<T>(l);
            outR[n] = static_cast<T>(r);
        }
    }

private:
    void apply() { fS.compute(fP, fSr); }

    double fSr = 48000.0;
    BandParams fP[kBands];
    EqSetup fS;
    double fZ[kBands][8] = {};
};

} // namespace nullpeq
