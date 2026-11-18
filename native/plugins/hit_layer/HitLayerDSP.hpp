/*
 * NULL JSFX - Hit Layer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/hit_layer.jsfx (computed in double, in the EEL2 order): a
 * transient detector (a fast and a slow envelope of the mono sum) starts a tuned sine with a
 * pitch drop and a burst of band-passed Park-Miller noise under every hit, added to the dry
 * signal. The two noise generators are exact Park-Miller steps, as rand() is everywhere else.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullhitlayer {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

// one Park-Miller step (x * 16807 mod 2^31 - 1, exact in doubles), as hl_pm()
inline double pm(double x) noexcept
{
    x *= 16807.0;
    return x - std::floor(x / 2147483647.0) * 2147483647.0;
}

struct Params {
    double thr = 0.0630957, tone = 60.0, tl = 0.5, tc = 0.0, pc = 0.0, drop = 12.0, nl = 0.3, nc = 0.0;
    double g = 0.0, k = 1.0 / 1.2, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double fol = 0.5, w = 0.0, dry = 1.0, out = 1.0;
};

// @slider, slider1..slider12 in order
inline Params paramsFor(const double* s, double sr) noexcept
{
    Params p;
    p.thr = std::pow(10.0, s[0] / 20.0);
    p.tone = s[1];
    p.tl = s[2] / 100.0;
    p.tc = std::exp(-6.907755 / (s[3] / 1000.0 * sr));
    p.pc = std::exp(-1.0 / (s[3] / 3000.0 * sr));
    p.drop = s[4];
    p.nl = s[5] / 100.0;
    p.nc = std::exp(-6.907755 / (s[6] / 1000.0 * sr));
    p.g = std::tan(kPi * std::min(s[7], 0.45 * sr) / sr);
    p.k = 1.0 / 1.2;
    p.a1 = 1.0 / (1.0 + p.g * (p.g + p.k));
    p.a2 = p.g * p.a1;
    p.a3 = p.g * p.a2;
    p.fol = s[8] / 100.0;
    p.w = s[9] / 100.0;
    p.dry = s[10] / 100.0;
    p.out = std::pow(10.0, s[11] / 20.0);
    return p;
}

class HitLayerProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr;
        fFa = std::exp(-1.0 / (0.0001 * sr));
        fFr = std::exp(-1.0 / (0.005 * sr));
        fSa = std::exp(-1.0 / (0.01 * sr));
        fSlr = std::exp(-1.0 / (0.1 * sr));
        reset();
        apply();
    }

    void reset() noexcept
    {
        fFast = fSlow = 0.0;
        fArmed = true;
        fHold = 0.0;
        fTe = fNe = fPe = 0.0;
        fPh = 0.0;
        fLv = 0.0;
        fSeed1 = 12345.0;
        fSeed2 = 54321.0;
        fIc1l = fIc2l = fIc1r = fIc2r = 0.0;
        fHit = false;
    }

    // slider1..slider12 of hit_layer.jsfx
    void setParams(const double* s) noexcept
    {
        std::copy(s, s + 12, fS);
        apply();
    }

    double sampleRate() const noexcept { return fSr; }
    double detector() const noexcept { return fFast; }
    double layerL() const noexcept { return fLl; }
    double layerR() const noexcept { return fLr; }
    // true for the one sample on which a hit started (display only)
    bool hit() const noexcept { return fHit; }

    void processSample(double& l, double& r) noexcept
    {
        const Params& p = fP;
        // the detector
        const double x = std::fabs(l + r) * 0.5;
        fFast = x > fFast ? fFa * fFast + (1.0 - fFa) * x : fFr * fFast + (1.0 - fFr) * x;
        fSlow = x > fSlow ? fSa * fSlow + (1.0 - fSa) * x : fSlr * fSlow + (1.0 - fSlr) * x;
        if (fHold > 0.0) fHold -= 1.0;
        if (!fArmed && fHold <= 0.0 && fFast < 1.5 * fSlow) fArmed = true;
        fHit = false;
        if (fArmed && fFast > p.thr && fFast > 2.0 * fSlow)
        {
            fArmed = false;
            fHold = std::floor(0.03 * fSr);
            const double st = std::max(0.0, std::min(1.0, 20.0 * std::log10(fFast / p.thr) / 24.0));
            fLv = (1.0 - p.fol) + p.fol * (0.25 + 0.75 * st);
            fTe = 1.0; fNe = 1.0; fPe = 1.0;
            fPh = 0.0;
            fHit = true;
        }

        // the tone: a sine falling from Pitch Drop above Tone to Tone
        const double f = p.tone * std::pow(2.0, p.drop * fPe / 12.0);
        fPh += f / fSr;
        if (fPh >= 1.0) fPh -= 1.0;
        const double t = std::sin(2.0 * kPi * fPh) * fTe * p.tl;
        fTe *= p.tc;
        fPe *= p.pc;
        if (fTe < 0.0000001) fTe = 0.0;
        if (fPe < 0.0000001) fPe = 0.0;

        // the noise: band-passed white noise (state-variable filter per channel)
        fSeed1 = pm(fSeed1);
        fSeed2 = pm(fSeed2);
        const double n1 = fSeed1 / 1073741824.0 - 1.0;
        const double n2 = n1 + (fSeed2 / 1073741824.0 - 1.0 - n1) * p.w;
        double v3 = n1 - fIc2l;
        double v1 = p.a1 * fIc1l + p.a2 * v3;
        double v2 = fIc2l + p.a2 * fIc1l + p.a3 * v3;
        fIc1l = 2.0 * v1 - fIc1l;
        fIc2l = 2.0 * v2 - fIc2l;
        const double bl = p.k * v1;
        v3 = n2 - fIc2r;
        v1 = p.a1 * fIc1r + p.a2 * v3;
        v2 = fIc2r + p.a2 * fIc1r + p.a3 * v3;
        fIc1r = 2.0 * v1 - fIc1r;
        fIc2r = 2.0 * v2 - fIc2r;
        const double br = p.k * v1;
        const double nk = fNe * p.nl;
        fNe *= p.nc;
        if (fNe < 0.0000001) fNe = 0.0;

        fLl = (t + bl * nk) * fLv;
        fLr = (t + br * nk) * fLv;
        l = (l * p.dry + fLl) * p.out;
        r = (r * p.dry + fLr) * p.out;
    }

private:
    void apply() noexcept { fP = paramsFor(fS, fSr); }

    double fS[12] = { -24.0, 60.0, 50.0, 200.0, 12.0, 30.0, 80.0, 4000.0, 50.0, 0.0, 100.0, 0.0 };
    Params fP;
    double fSr = 44100.0;
    double fFa = 0.0, fFr = 0.0, fSa = 0.0, fSlr = 0.0;
    double fFast = 0.0, fSlow = 0.0, fHold = 0.0;
    bool fArmed = true, fHit = false;
    double fTe = 0.0, fNe = 0.0, fPe = 0.0, fPh = 0.0, fLv = 0.0;
    double fSeed1 = 12345.0, fSeed2 = 54321.0;
    double fIc1l = 0.0, fIc2l = 0.0, fIc1r = 0.0, fIc2r = 0.0;
    double fLl = 0.0, fLr = 0.0;
};

} // namespace nullhitlayer
