/*
 * NULL JSFX - De-Plosive, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/de_plosive.jsfx, computed in double like EEL2: a Linkwitz-Riley
 * 4 crossover at Frequency (three TPT state-variable filters per channel), a linked detector
 * (fast low and high envelopes, slow low average) that turns the low band alone down when it
 * jumps above both by more than the Sensitivity threshold, and a 5 ms look-ahead delay on both
 * bands (the latency). Listen outputs only the part of the low band that is removed.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulldeplosive {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr double kSqrt2 = 1.4142135623730951;

struct Params {
    double sensitivity = 50.0, frequency = 150.0, maxReduction = 18.0, release = 80.0;
    int listen = 0;
};

// look-ahead in samples at a sample rate (the JSFX's pdc_delay)
inline int lookaheadFor(double sr) noexcept { return int(std::floor(0.005 * sr + 0.5)); }

struct Svf {
    double ic1 = 0.0, ic2 = 0.0, lp = 0.0, hp = 0.0;
    void process(double x, double a1, double a2, double a3) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        lp = v2;
        hp = x - kSqrt2 * v1 - v2;
    }
    void flush() noexcept
    {
        if (std::fabs(ic1) < 0.000000000000001) ic1 = 0.0;
        if (std::fabs(ic2) < 0.000000000000001) ic2 = 0.0;
    }
};

class DePlosiveProcessor {
public:
    // @init (allocates; call outside the audio thread)
    void init(double sr)
    {
        fSr = sr;
        fLook = lookaheadFor(sr);
        fBuf.assign(size_t(std::max(1, 4 * fLook)), 0.0);
        fWp = 0;
        fAttE = std::exp(-1.0 / (0.001 * sr));
        fRelE = std::exp(-1.0 / (0.03 * sr));
        fSlowE = std::exp(-1.0 / (0.2 * sr));
        fAttG = std::exp(-1.0 / (0.001 * sr));
        fELo = fEHi = fSLo = 0.0;
        fGs = 0.0;
        fGl = 1.0;
        for (Svf& s : fF)
            s = Svf();
    }

    // @slider
    void setParams(const Params& p) noexcept
    {
        const double sg = std::tan(kPi * p.frequency / fSr);
        fA1 = 1.0 / (1.0 + sg * (sg + kSqrt2));
        fA2 = sg * fA1;
        fA3 = sg * fA2;
        fThrDb = 12.0 - 0.12 * p.sensitivity;
        fThr = std::pow(10.0, fThrDb / 20.0);
        fMaxR = p.maxReduction;
        fRelG = std::exp(-1.0 / (p.release * 0.001 * fSr));
        fListen = p.listen;
    }

    int latency() const noexcept { return fLook; }
    double reductionDb() const noexcept { return fGs; }  // gs: the low band's cut now, in dB
    double lowGain() const noexcept { return fGl; }      // gl
    double delayedLowPeak() const noexcept { return fLowPeak; } // max(|dlo_l|, |dlo_r|)

    // @block
    void block() noexcept
    {
        for (Svf& s : fF)
            s.flush();
        if (fELo < 0.000000000000001) fELo = 0.0;
        if (fEHi < 0.000000000000001) fEHi = 0.0;
        if (fSLo < 0.000000000000001) fSLo = 0.0;
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        // Linkwitz-Riley 4: low = LP(LP(x)), high = HP(HP(x))
        Svf &l1 = fF[0], &l2 = fF[1], &l3 = fF[2], &r1 = fF[3], &r2 = fF[4], &r3 = fF[5];
        l1.process(l, fA1, fA2, fA3);
        l2.process(l1.lp, fA1, fA2, fA3);
        l3.process(l1.hp, fA1, fA2, fA3);
        r1.process(r, fA1, fA2, fA3);
        r2.process(r1.lp, fA1, fA2, fA3);
        r3.process(r1.hp, fA1, fA2, fA3);
        const double loL = l2.lp, hiL = l3.hp, loR = r2.lp, hiR = r3.hp;

        // detector: fast low and high envelopes, slow low average
        const double dl = std::max(std::fabs(loL), std::fabs(loR));
        const double dh = std::max(std::fabs(hiL), std::fabs(hiR));
        fELo = dl > fELo ? dl + (fELo - dl) * fAttE : dl + (fELo - dl) * fRelE;
        fEHi = dh > fEHi ? dh + (fEHi - dh) * fAttE : dh + (fEHi - dh) * fRelE;
        fSLo = fELo + (fSLo - fELo) * fSlowE;
        const double ratio = std::min(fELo / (fEHi + 0.001), fELo / (fSLo + 0.001));
        const double gt = ratio > fThr ? std::min(fMaxR, 2.0 * (20.0 * std::log10(ratio) - fThrDb)) : 0.0;
        fGs = gt > fGs ? gt + (fGs - gt) * fAttG : gt + (fGs - gt) * fRelG;
        if (fGs < 0.000001)
            fGs = 0.0;
        fGl = fGs > 0.0 ? std::pow(10.0, 0.0 - fGs / 20.0) : 1.0;

        // look-ahead: both bands 5 ms late
        double* b = fBuf.data();
        const int n = fLook;
        const double dloL = b[fWp]; b[fWp] = loL;
        const double dloR = b[n + fWp]; b[n + fWp] = loR;
        const double dhiL = b[2 * n + fWp]; b[2 * n + fWp] = hiL;
        const double dhiR = b[3 * n + fWp]; b[3 * n + fWp] = hiR;
        fWp = fWp + 1 >= n ? 0 : fWp + 1;
        fLowPeak = std::max(std::fabs(dloL), std::fabs(dloR));

        if (fListen > 0)
        {
            l = dloL * (1.0 - fGl);
            r = dloR * (1.0 - fGl);
        }
        else
        {
            l = dloL * fGl + dhiL;
            r = dloR * fGl + dhiR;
        }
    }

private:
    double fSr = 48000.0;
    int fLook = 240, fWp = 0, fListen = 0;
    std::vector<double> fBuf;
    Svf fF[6];
    double fA1 = 1.0, fA2 = 0.0, fA3 = 0.0;
    double fThrDb = 6.0, fThr = 2.0, fMaxR = 18.0, fRelG = 0.0;
    double fAttE = 0.0, fRelE = 0.0, fSlowE = 0.0, fAttG = 0.0;
    double fELo = 0.0, fEHi = 0.0, fSLo = 0.0, fGs = 0.0, fGl = 1.0, fLowPeak = 0.0;
};

} // namespace nulldeplosive
