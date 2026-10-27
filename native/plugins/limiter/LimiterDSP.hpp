/*
 * NULL JSFX - Limiter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/limiter.jsfx, computed in double like EEL2. Threshold drives
 * the signal so that its level comes out at Ceiling. Limiter: look-ahead peak limiter (the
 * gain starts falling Lookahead ms before a peak, reported as latency). Clipper: soft clipper
 * (linear to 70% of the ceiling, then an exponential knee), no delay. Brickwall: like Limiter
 * with an instant gain drop.
 *
 * Appended in 2.7: True Peak (Limiter and Brickwall: the detector sees the true peak from
 * nulljsfx::TruePeak, 6 samples late, and the audio waits 6 samples more) and Listen Delta (the
 * driven input minus the output). Off, every operation is the original's.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "NullTruePeak.hpp"

namespace nulllimiter {

class LimiterProcessor {
public:
    // @init: the look-ahead buffers (11 ms + 2), cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fMaxLook = int(std::ceil(sr * 0.011)) + 2 + 6;
        fL.assign(size_t(fMaxLook), 0.0);
        fR.assign(size_t(fMaxLook), 0.0);
        fW = 0;
        fHoldVal = 0.0;
        fHoldCnt = 0.0;
        fGain = 1.0;
        fTp.reset();
    }

    // @slider
    void setParams(int mode, double thresholdDb, double ceilingDb, double releaseMs, double lookaheadMs,
                   bool truePeak = false, bool delta = false)
    {
        fMode = mode;
        fTpk = mode != 1 && truePeak;
        fDelta = delta;
        fCeiling = std::pow(10.0, ceilingDb / 20.0);
        fDrive = std::pow(10.0, (ceilingDb - thresholdDb) / 20.0);
        fLook = mode == 1 ? 0 : int(std::floor(lookaheadMs * fSr / 1000.0 + 0.5));
        fRel = std::exp(-1.0 / std::max(1.0, releaseMs * fSr / 1000.0));
        fAtt = mode == 2 || fLook < 1 ? 1.0 : 1.0 - std::exp(-5.0 / double(fLook));
    }

    int latency() const noexcept { return fLook + (fTpk ? 6 : 0); } // pdc_delay
    int mode() const noexcept { return fMode; }
    double drive() const noexcept { return fDrive; }
    double gain() const noexcept { return fGain; }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        double il = l * fDrive, ir = r * fDrive;
        if (fMode == 1)
        {
            const double knee = 0.7 * fCeiling;
            double a = std::fabs(il);
            if (a > knee)
                il = sign(il) * (knee + (fCeiling - knee) * (1.0 - std::exp(-(a - knee) / (fCeiling - knee))));
            a = std::fabs(ir);
            if (a > knee)
                ir = sign(ir) * (knee + (fCeiling - knee) * (1.0 - std::exp(-(a - knee) / (fCeiling - knee))));
            if (fDelta)
            {
                l = l * fDrive - il;
                r = r * fDrive - ir;
            }
            else
            {
                l = il;
                r = ir;
            }
            return;
        }
        const double peak = fTpk ? fTp.peak(il, ir, true) : std::max(std::fabs(il), std::fabs(ir));
        if (peak >= fHoldVal)
        {
            fHoldVal = peak;
            fHoldCnt = fLook;
        }
        else if (fHoldCnt > 0.0)
            fHoldCnt -= 1.0;
        else
            fHoldVal = std::max(peak, fHoldVal * fRel);
        const double target = fHoldVal > fCeiling ? fCeiling / fHoldVal : 1.0;
        fGain = target < fGain ? fGain + (target - fGain) * fAtt : target + (fGain - target) * fRel;

        fL[size_t(fW)] = il;
        fR[size_t(fW)] = ir;
        int rp = fW - fLook - (fTpk ? 6 : 0);
        if (rp < 0)
            rp += fMaxLook;
        if (++fW >= fMaxLook)
            fW = 0;
        l = std::max(-fCeiling, std::min(fCeiling, fL[size_t(rp)] * fGain));
        r = std::max(-fCeiling, std::min(fCeiling, fR[size_t(rp)] * fGain));
        if (fDelta)
        {
            l = fL[size_t(rp)] - l;
            r = fR[size_t(rp)] - r;
        }
    }

private:
    static double sign(double x) noexcept { return x > 0.0 ? 1.0 : x < 0.0 ? -1.0 : 0.0; }

    double fSr = 48000.0;
    std::vector<double> fL, fR;
    int fMaxLook = 1, fW = 0, fLook = 0, fMode = 0;
    double fHoldVal = 0.0, fHoldCnt = 0.0, fGain = 1.0;
    double fCeiling = 1.0, fDrive = 1.0, fRel = 0.0, fAtt = 1.0;
    nulljsfx::TruePeak fTp;
    bool fTpk = false, fDelta = false;
};

} // namespace nulllimiter
