/*
 * NULL JSFX - De-Hum, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/de_hum.jsfx, computed in double like EEL2: an optional 24 dB/oct
 * TPT high-pass (Low Cut), then Harmonics notch biquads (transposed direct form II) at
 * h * (50 or 60 Hz + Fine). Each notch has the poles of a band of width f / Q and its zeros
 * pulled in to the gain 10^(-Depth/20) at the centre.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulldehum {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kMaxNotches = 8;

struct Params {
    int mains = 0;      // 0 = 50 Hz, 1 = 60 Hz
    double harmonics = 4.0, q = 30.0, depth = 40.0, fine = 0.0;
    int lowCut = 0;     // 0 off, 1..3 = 20, 30, 40 Hz
};

// coefficients of one TPT SVF stage
struct Stage {
    double k = 1.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    void set(double g, double q) noexcept
    {
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct Svf {
    double ic1 = 0.0, ic2 = 0.0;
    double hp(double x, const Stage& c) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return x - c.k * v1 - v2;
    }
    void flush() noexcept
    {
        if (std::fabs(ic1) < 0.000000000000001) ic1 = 0.0;
        if (std::fabs(ic2) < 0.000000000000001) ic2 = 0.0;
    }
};

// everything the JSFX's @slider computes
struct Coeffs {
    double f0 = 50.0, nh = 4.0;
    int lc = 0;
    double co[kMaxNotches][5] = {}; // b0, b1, b2, a1, a2
    Stage lc1, lc2;

    void compute(const Params& p, double sr) noexcept
    {
        f0 = (p.mains < 1 ? 50.0 : 60.0) + p.fine;
        nh = p.harmonics;
        const double gd = std::pow(10.0, 0.0 - p.depth / 20.0);
        for (int h = 0; h < kMaxNotches; ++h)
        {
            const double w = 2.0 * kPi * std::min(f0 * (h + 1), sr * 0.45) / sr;
            const double al = std::sin(w) / (2.0 * p.q);
            const double cw = std::cos(w);
            const double a0 = 1.0 + al;
            co[h][0] = (1.0 + al * gd) / a0;
            co[h][1] = -2.0 * cw / a0;
            co[h][2] = (1.0 - al * gd) / a0;
            co[h][3] = -2.0 * cw / a0;
            co[h][4] = (1.0 - al) / a0;
        }
        lc = p.lowCut;
        if (lc > 0)
        {
            const double g = std::tan(kPi * (10.0 + 10.0 * lc) / sr);
            lc1.set(g, 0.5412);
            lc2.set(g, 1.3066);
        }
    }

    int numNotches() const noexcept { return std::max(0, std::min(kMaxNotches, int(nh))); }

    // display: magnitude (dB) of the whole chain at f (dh_db in the GFX JSFX)
    double responseDb(double f, double sr) const noexcept
    {
        const double w = 2.0 * kPi * std::min(f, sr * 0.4999) / sr;
        const double cr = std::cos(w), ci = 0.0 - std::sin(w);
        const double c2r = std::cos(2.0 * w), c2i = 0.0 - std::sin(2.0 * w);
        double m = 0.0;
        for (int h = 0; h < numNotches(); ++h)
        {
            const double* c = co[h];
            const double br = c[0] + c[1] * cr + c[2] * c2r;
            const double bi = c[1] * ci + c[2] * c2i;
            const double ar = 1.0 + c[3] * cr + c[4] * c2r;
            const double ai = c[3] * ci + c[4] * c2i;
            m += 10.0 * std::log10(std::max(br * br + bi * bi, 1e-30) / std::max(ar * ar + ai * ai, 1e-30));
        }
        if (lc > 0)
        {
            double o = std::tan(kPi * std::min(f, sr * 0.4999) / sr) / std::tan(kPi * (10.0 + 10.0 * lc) / sr);
            o *= o;
            m += 10.0 * std::log10(std::max(o * o / ((1.0 - o) * (1.0 - o) + o / (0.5412 * 0.5412)), 1e-30));
            m += 10.0 * std::log10(std::max(o * o / ((1.0 - o) * (1.0 - o) + o / (1.3066 * 1.3066)), 1e-30));
        }
        return m;
    }
};

class DeHumProcessor {
public:
    // @init
    void init(double sr) noexcept
    {
        fSr = sr;
        for (auto& s : fSt)
            for (double& v : s)
                v = 0.0;
        fLcL1 = fLcL2 = fLcR1 = fLcR2 = Svf();
    }

    // @slider
    void setParams(const Params& p) noexcept
    {
        fC.compute(p, fSr);
        // notches that are off start from silence when they come back
        for (int h = fC.numNotches(); h < kMaxNotches; ++h)
            for (double& v : fSt[h])
                v = 0.0;
        if (fC.lc <= 0)
            fLcL1 = fLcL2 = fLcR1 = fLcR2 = Svf();
    }

    const Coeffs& coeffs() const noexcept { return fC; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        // @block
        fLcL1.flush(); fLcL2.flush(); fLcR1.flush(); fLcR2.flush();
        for (auto& s : fSt)
            for (double& v : s)
                if (std::fabs(v) < 0.000000000000001)
                    v = 0.0;

        const int nh = fC.numNotches();
        for (uint32_t i = 0; i < frames; ++i)
        {
            double xl = inL[i], xr = inR[i];
            if (fC.lc > 0)
            {
                xl = fLcL2.hp(fLcL1.hp(xl, fC.lc1), fC.lc2);
                xr = fLcR2.hp(fLcR1.hp(xr, fC.lc1), fC.lc2);
            }
            for (int h = 0; h < nh; ++h)
            {
                const double* c = fC.co[h];
                double* s = fSt[h];
                double yo = c[0] * xl + s[0];
                s[0] = c[1] * xl - c[3] * yo + s[1];
                s[1] = c[2] * xl - c[4] * yo;
                xl = yo;
                yo = c[0] * xr + s[2];
                s[2] = c[1] * xr - c[3] * yo + s[3];
                s[3] = c[2] * xr - c[4] * yo;
                xr = yo;
            }
            outL[i] = static_cast<T>(xl);
            outR[i] = static_cast<T>(xr);
        }
    }

private:
    double fSr = 48000.0;
    Coeffs fC;
    double fSt[kMaxNotches][4] = {};
    Svf fLcL1, fLcL2, fLcR1, fLcR2;
};

} // namespace nulldehum
