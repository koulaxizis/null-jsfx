/*
 * NULL JSFX - Pressure, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/pressure.jsfx, computed in double like EEL2:
 *
 *   thr = -30a dB, ratio = 1 + 5a, makeup = 10^(-thr*(1 - 1/ratio)*0.5/20), a = Amount/100
 *   env = det*env + (1 - det)*max(l^2, r^2)                   (5 ms detector)
 *   db  = 10 log10(env + 1e-10)
 *   target = db > thr ? 10^(-(db - thr)*(1 - 1/ratio)/20) : 1
 *   gr  follows target with 10 ms attack / 150 ms release; y = x*gr*makeup
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullpressure {

struct Params {
    double a, thr, ratio, makeup;
};

// the JSFX @slider section (level part; the time constants are set per sample rate)
inline Params paramsFor(double slider) noexcept
{
    Params p;
    p.a = slider / 100.0;
    p.thr = -30.0 * p.a;
    p.ratio = 1.0 + 5.0 * p.a;
    p.makeup = std::pow(10.0, -p.thr * (1.0 - 1.0 / p.ratio) * 0.5 / 20.0);
    return p;
}

// static curve in dB (gain computer plus makeup), for the display
inline double curveDb(const Params& p, double d) noexcept
{
    return d - (d > p.thr ? (d - p.thr) * (1.0 - 1.0 / p.ratio) : 0.0) - p.thr * (1.0 - 1.0 / p.ratio) * 0.5;
}

class PressureProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fAtt = std::exp(-1.0 / (0.01 * sr));
        fRel = std::exp(-1.0 / (0.15 * sr));
        fDet = std::exp(-1.0 / (0.005 * sr));
    }
    void setAmount(double slider) noexcept { fP = paramsFor(slider); }
    void reset() noexcept { fEnv = 0.0; fGr = 1.0; fDb = -100.0; }

    // current gain (linear) and detector level (dB), for the meters
    double gain() const noexcept { return fGr; }
    double detectorDb() const noexcept { return fDb; }

    inline void processSample(double& l, double& r) noexcept
    {
        const Params& p = fP;
        const double lev = std::fmax(l * l, r * r);
        fEnv = fDet * fEnv + (1.0 - fDet) * lev;
        const double db = 10.0 * std::log10(fEnv + 0.0000000001);
        fDb = db;
        const double target = db > p.thr ? std::pow(10.0, -(db - p.thr) * (1.0 - 1.0 / p.ratio) / 20.0) : 1.0;
        fGr = target < fGr ? fAtt * fGr + (1.0 - fAtt) * target : fRel * fGr + (1.0 - fRel) * target;
        l *= fGr * p.makeup;
        r *= fGr * p.makeup;
    }

private:
    Params fP = paramsFor(50.0);
    double fAtt = 0.0, fRel = 0.0, fDet = 0.0;
    double fEnv = 0.0, fGr = 1.0, fDb = -100.0;
};

} // namespace nullpressure
