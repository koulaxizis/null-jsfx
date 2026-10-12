/*
 * NULL JSFX - OTT, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/ott.jsfx, computed in double like EEL2: three bands split
 * with 4th-order Linkwitz-Riley low-passes at 88 Hz and 2.5 kHz (high = rest), each with a
 * linked envelope that pushes levels above -33 dB down (66:1) and lifts levels between -80
 * and -41 dB (4:1, up to +24 dB), band gains, fixed make-up, depth blend, output gain and a
 * soft safety bend above 0 dBFS.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullott {

// the gain computer of one band in dB for the level d dB (as the JSFX's band_gain)
inline double bandGainDb(double d, double up, double down) noexcept
{
    double g = 0.0;
    if (d > -33.0)
        g -= (d + 33.0) * (1.0 - 1.0 / 66.0) * down;
    if (d < -41.0 && d > -80.0)
        g += std::min(24.0, (-41.0 - d) * (1.0 - 1.0 / 4.0)) * up;
    return g;
}

// 2nd-order Butterworth low-pass (lp_set / lp_run)
struct LowPass {
    double b0 = 0, b1 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
    void set(double freq, double sr) noexcept
    {
        const double k = std::tan(M_PI * freq / sr);
        const double n = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
        b0 = k * k * n;
        b1 = 2.0 * b0;
        a1 = 2.0 * (k * k - 1.0) * n;
        a2 = (1.0 - std::sqrt(2.0) * k + k * k) * n;
    }
    double run(double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b0 * x - a2 * y;
        return y;
    }
};

class OttProcessor {
public:
    // @init: everything starts at 0 (EEL2 variables)
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (LowPass* f : { &a1l, &a2l, &a1r, &a2r, &b1l, &b2l, &b1r, &b2r })
            *f = LowPass();
        fEnv[0] = fEnv[1] = fEnv[2] = 0.0;
        fGain[0] = fGain[1] = fGain[2] = 1.0;
    }

    // @slider
    void setParams(double depthPct, double timePct, double upPct, double downPct, double lowDb, double midDb,
                   double highDb, double outDb)
    {
        for (LowPass* f : { &a1l, &a2l, &a1r, &a2r })
            f->set(88.0, fSr);
        for (LowPass* f : { &b1l, &b2l, &b1r, &b2r })
            f->set(2500.0, fSr);
        fDepth = depthPct / 100.0;
        const double t = std::max(timePct, 1.0) / 100.0;
        fAtt = std::exp(-1.0 / (0.0135 * t * fSr));
        fRel = std::exp(-1.0 / (0.132 * t * fSr));
        fUp = upPct / 100.0;
        fDown = downPct / 100.0;
        fBand[0] = std::pow(10.0, lowDb / 20.0);
        fBand[1] = std::pow(10.0, midDb / 20.0);
        fBand[2] = std::pow(10.0, highDb / 20.0);
        fMakeup = std::pow(10.0, 12.0 * fDown / 20.0);
        fOut = std::pow(10.0, outDb / 20.0);
    }

    double up() const noexcept { return fUp; }
    double down() const noexcept { return fDown; }
    // the latest gain of band 0 (low), 1 (mid), 2 (high), band gain included (gl, gm, gh)
    double bandGain(int b) const noexcept { return fGain[b]; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            const double loL = a2l.run(a1l.run(xl));
            const double loR = a2r.run(a1r.run(xr));
            const double restL = xl - loL, restR = xr - loR;
            const double midL = b2l.run(b1l.run(restL));
            const double midR = b2r.run(b1r.run(restR));
            const double hiL = restL - midL, hiR = restR - midR;

            const double gl = gain(0, std::max(std::fabs(loL), std::fabs(loR))) * fBand[0];
            const double gm = gain(1, std::max(std::fabs(midL), std::fabs(midR))) * fBand[1];
            const double gh = gain(2, std::max(std::fabs(hiL), std::fabs(hiR))) * fBand[2];
            fGain[0] = gl;
            fGain[1] = gm;
            fGain[2] = gh;

            const double wetL = (loL * gl + midL * gm + hiL * gh) * fMakeup;
            const double wetR = (loR * gl + midR * gm + hiR * gh) * fMakeup;
            outL[i] = static_cast<T>(bend((xl * (1.0 - fDepth) + wetL * fDepth) * fOut));
            outR[i] = static_cast<T>(bend((xr * (1.0 - fDepth) + wetR * fDepth) * fOut));
        }
    }

private:
    // band_gain: envelope, then 10 ^ (gain dB / 20)
    double gain(int b, double x) noexcept
    {
        const double lev = std::fabs(x);
        double& env = fEnv[b];
        env = lev > env ? fAtt * env + (1.0 - fAtt) * lev : fRel * env + (1.0 - fRel) * lev;
        const double db = 20.0 * std::log10(std::max(env, 0.000001));
        return std::pow(10.0, bandGainDb(db, fUp, fDown) / 20.0);
    }

    // above 0 dBFS, bend towards +6 dBFS
    static double bend(double s) noexcept
    {
        if (std::fabs(s) > 1.0)
            s = (s > 0.0 ? 1.0 : -1.0) * (1.0 + (std::fabs(s) - 1.0) / std::fabs(s));
        return s;
    }

    double fSr = 48000.0;
    LowPass a1l, a2l, a1r, a2r, b1l, b2l, b1r, b2r;
    double fEnv[3] = {}, fGain[3] = { 1.0, 1.0, 1.0 }, fBand[3] = { 1.0, 1.0, 1.0 };
    double fDepth = 1.0, fAtt = 0.0, fRel = 0.0, fUp = 1.0, fDown = 1.0, fMakeup = 1.0, fOut = 1.0;
};

} // namespace nullott
