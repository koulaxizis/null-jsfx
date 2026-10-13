/*
 * NULL JSFX - Disperser, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/disperser.jsfx, computed in double like EEL2: Amount (0..64)
 * TPT state-variable all-passes (y = x - 2 k bp) per channel, all at one frequency and Q.
 * Targets (log frequency per channel, log Q, mix) glide by a 20 ms one-pole and snap within 1e-4;
 * Stereo puts the left channel at 0.8409 x Frequency and the right at 1.1892 x.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulldisperser {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kMaxStages = 64;

struct Params {
    double freq = 1000.0, amount = 16.0, pinch = 50.0, mix = 100.0;
    int mode = 0;
};

// the JSFX's @slider targets
struct Targets {
    double tl = 0.0, tr = 0.0, tq = 0.0, tm = 1.0;
    int n = 16;

    void compute(const Params& p, double sr) noexcept
    {
        tl = std::log(std::min(p.freq * (p.mode > 0 ? 0.8409 : 1.0), sr * 0.45));
        tr = std::log(std::min(p.freq * (p.mode > 0 ? 1.1892 : 1.0), sr * 0.45));
        tq = std::log(0.25 * std::pow(40.0, p.pinch / 100.0));
        tm = p.mix / 100.0;
        n = std::max(0, std::min(kMaxStages, int(p.amount)));
    }

    // display: group delay (ms) of the chain at f, all-passes at the log frequency lf (dp_ms in
    // the GFX JSFX): 2 k (1 + o^2) / ((1 - o^2)^2 + k^2 o^2) times d o / d w = (1 + t^2) / (2 g)
    double delayMs(double f, double lf, double sr) const noexcept
    {
        const double t = std::tan(kPi * std::min(f, sr * 0.4999) / sr);
        const double g = std::tan(kPi * std::exp(lf) / sr);
        const double o2 = t * t / (g * g);
        const double kk = 1.0 / std::exp(tq);
        return n * 2.0 * kk * (1.0 + o2) / ((1.0 - o2) * (1.0 - o2) + kk * kk * o2) * (1.0 + t * t) / (2.0 * g)
               / sr * 1000.0;
    }
};

class DisperserProcessor {
public:
    // @init
    void init(double sr) noexcept
    {
        fSr = sr;
        for (double& v : fStL) v = 0.0;
        for (double& v : fStR) v = 0.0;
        fKs = 1.0 - std::exp(-1.0 / (0.02 * sr));
        fSnap = true;
    }

    // @slider
    void setParams(const Params& p) noexcept
    {
        fT.compute(p, fSr);
        // stages that are off start from silence when they come back
        for (int i = fT.n * 2; i < kMaxStages * 2; ++i)
            fStL[i] = fStR[i] = 0.0;
        if (fSnap)
        {
            fLfl = fT.tl;
            fLfr = fT.tr;
            fLq = fT.tq;
            fMx = fT.tm;
            coefs();
            fSnap = false;
        }
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        // @block: flush states that decayed below -300 dB
        for (double& v : fStL)
            if (std::fabs(v) < 0.000000000000001) v = 0.0;
        for (double& v : fStR)
            if (std::fabs(v) < 0.000000000000001) v = 0.0;

        const int n = fT.n;
        for (uint32_t i = 0; i < frames; ++i)
        {
            if (fLfl != fT.tl || fLfr != fT.tr || fLq != fT.tq || fMx != fT.tm)
            {
                fLfl += (fT.tl - fLfl) * fKs;
                if (std::fabs(fT.tl - fLfl) < 0.0001) fLfl = fT.tl;
                fLfr += (fT.tr - fLfr) * fKs;
                if (std::fabs(fT.tr - fLfr) < 0.0001) fLfr = fT.tr;
                fLq += (fT.tq - fLq) * fKs;
                if (std::fabs(fT.tq - fLq) < 0.0001) fLq = fT.tq;
                fMx += (fT.tm - fMx) * fKs;
                if (std::fabs(fT.tm - fMx) < 0.0001) fMx = fT.tm;
                coefs();
            }
            const double inl = inL[i], inr = inR[i];
            const double xl = chain(inl, fStL, fAl1, fAl2, fAl3, n);
            const double xr = chain(inr, fStR, fAr1, fAr2, fAr3, n);
            outL[i] = static_cast<T>(inl + (xl - inl) * fMx);
            outR[i] = static_cast<T>(inr + (xr - inr) * fMx);
        }
    }

private:
    void coefs() noexcept
    {
        fK = 1.0 / std::exp(fLq);
        double g = std::tan(kPi * std::exp(fLfl) / fSr);
        fAl1 = 1.0 / (1.0 + g * (g + fK));
        fAl2 = g * fAl1;
        fAl3 = g * fAl2;
        g = std::tan(kPi * std::exp(fLfr) / fSr);
        fAr1 = 1.0 / (1.0 + g * (g + fK));
        fAr2 = g * fAr1;
        fAr3 = g * fAr2;
    }

    double chain(double x, double* s, double a1, double a2, double a3, int n) const noexcept
    {
        const double k2 = 2.0 * fK;
        for (int j = 0; j < n; ++j, s += 2)
        {
            const double v3 = x - s[1];
            const double v1 = a1 * s[0] + a2 * v3;
            const double v2 = s[1] + a2 * s[0] + a3 * v3;
            s[0] = 2.0 * v1 - s[0];
            s[1] = 2.0 * v2 - s[1];
            x -= k2 * v1;
        }
        return x;
    }

    double fSr = 48000.0, fKs = 0.0;
    bool fSnap = true;
    Targets fT;
    double fLfl = 0.0, fLfr = 0.0, fLq = 0.0, fMx = 1.0;
    double fK = 1.0, fAl1 = 1.0, fAl2 = 0.0, fAl3 = 0.0, fAr1 = 1.0, fAr2 = 0.0, fAr3 = 0.0;
    double fStL[kMaxStages * 2] = {};
    double fStR[kMaxStages * 2] = {};
};

} // namespace nulldisperser
