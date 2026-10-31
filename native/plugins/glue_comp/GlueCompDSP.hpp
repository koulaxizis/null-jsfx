/*
 * NULL JSFX - Glue Comp, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/glue_comp.jsfx, computed in double in the EEL2 order: a
 * stereo-linked peak detector (after an optional one-pole high-pass on the key), a gain
 * computer with a 4 dB soft knee, gain reduction smoothed in dB (Auto release: the deeper of
 * a fast 80 ms envelope and a slow 0.5 s / 1.5 s one), then Makeup and the dry / wet Mix.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullgluecomp {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    double threshold = -10.0, makeup = 0.0, mix = 100.0, hpf = 0.0;
    int ratio = 0, attack = 4, release = 4;
};

struct Coeffs {
    double thr = -10.0, slope = 0.5, ca = 1.0, cr = 1.0, csa = 1.0, csr = 1.0, mk = 1.0, mix = 1.0, ch = 0.0;
    bool relAuto = true, hpf = false;
};

// the @slider code of glue_comp.jsfx
inline Coeffs coeffsFor(const Params& p, double sr) noexcept
{
    Coeffs c;
    c.thr = p.threshold;
    c.slope = 1.0 - 1.0 / (p.ratio == 0 ? 2.0 : p.ratio == 1 ? 4.0 : 10.0);
    const int a = p.attack, r = p.release;
    const double att = a == 0 ? 0.1 : a == 1 ? 0.3 : a == 2 ? 1.0 : a == 3 ? 3.0 : a == 4 ? 10.0 : 30.0;
    const double rel = r == 0 ? 0.1 : r == 1 ? 0.3 : r == 2 ? 0.6 : r == 3 ? 1.2 : 0.08;
    c.relAuto = r >= 4;
    c.ca = 1.0 - std::exp(-1.0 / (att * 0.001 * sr));
    c.cr = 1.0 - std::exp(-1.0 / (rel * sr));
    c.csa = 1.0 - std::exp(-1.0 / (0.5 * sr));
    c.csr = 1.0 - std::exp(-1.0 / (1.5 * sr));
    c.mk = std::pow(10.0, p.makeup / 20.0);
    c.mix = p.mix / 100.0;
    c.hpf = p.hpf > 0.0;
    c.ch = std::exp(-2.0 * kPi * std::fmin(std::fmax(p.hpf, 1.0), 0.45 * sr) / sr);
    return c;
}

// the gain computer: reduction in dB for a level x dB above Threshold
inline double reductionDb(double slope, double x) noexcept
{
    return x <= -2.0 ? 0.0 : x < 2.0 ? slope * (x + 2.0) * (x + 2.0) / 8.0 : slope * x;
}

// the static curve: output level in dB for a steady input level d (gc_curve in glue_comp_gfx.jsfx)
inline double curveDb(const Coeffs& c, double makeupDb, double d) noexcept
{
    return d - reductionDb(c.slope, d - c.thr) + makeupDb;
}

class GlueCompProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        fC = coeffsFor(fP, fSr);
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fC = coeffsFor(fP, fSr);
    }

    void reset() noexcept { fHl = fHr = fGrf = fGrs = fGr = 0.0; }

    // the gain reduction in dB and the detector's level (linear), for the meters
    double gr() const noexcept { return fGr; }
    double keyAbs() const noexcept { return fKey; }

    void processSample(double& l, double& r) noexcept
    {
        const Coeffs& c = fC;
        double kl = l, kr = r;
        if (c.hpf)
        {
            fHl = (1.0 - c.ch) * kl + c.ch * fHl;
            fHr = (1.0 - c.ch) * kr + c.ch * fHr;
            kl -= fHl;
            kr -= fHr;
        }
        fKey = std::fmax(std::fabs(kl), std::fabs(kr));
        const double lv = 20.0 * std::log10(std::fmax(fKey, 0.000001));
        const double tg = reductionDb(c.slope, lv - c.thr);
        fGrf += (tg - fGrf) * (tg > fGrf ? c.ca : c.cr);
        if (fGrf < 0.000000001)
            fGrf = 0.0;
        if (c.relAuto)
        {
            fGrs += (tg - fGrs) * (tg > fGrs ? c.csa : c.csr);
            if (fGrs < 0.000000001)
                fGrs = 0.0;
            fGr = std::fmax(fGrf, fGrs);
        }
        else
            fGr = fGrf;
        const double g = 1.0 - c.mix + c.mix * c.mk * std::pow(10.0, -fGr / 20.0);
        l *= g;
        r *= g;
    }

private:
    double fSr = 48000.0;
    Params fP;
    Coeffs fC = coeffsFor(Params(), 48000.0);
    double fHl = 0.0, fHr = 0.0, fGrf = 0.0, fGrs = 0.0, fGr = 0.0, fKey = 0.0;
};

} // namespace nullgluecomp
