/*
 * NULL JSFX - Gate, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/gate.jsfx, computed in double like EEL2: a stereo-linked peak
 * detector (4.5 ms fall) on an optional high-passed sidechain opens the gate above Threshold
 * (Attack) and closes it below (Release) down to Range (-80 dB = fully closed). Hold keeps it
 * open that long after the level falls; Lookahead delays the audio (not the detector), reported as
 * latency; External listens to a key (JSFX pins 3/4); Sidechain LPF low-passes the detector (off at
 * 20 kHz); Flip makes it a ducker (down to Range above Threshold).
 */

#pragma once

#include <cmath>
#include <vector>

namespace nullgate {

// a 0.1-step parameter back on the JSFX grid (Lookahead), in double
inline double onGrid(double v) noexcept { return std::round(v * 10.0) / 10.0; }

class GateProcessor {
public:
    // @init (with the JSFX's hard-coded defaults, which @slider then overrides)
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPkDecay = std::exp(-1.0 / (0.0045 * sr));
        fScHpf = 0.0;
        fThresholdLin = std::pow(10.0, -40.0 / 20.0);
        coefs(10.0, 200.0);
        fGateLevel = 0.0;
        fEnvL = fEnvR = 0.0;
        fLpfL = fLpfR = 0.0;
        fLpfCoef = 0.0;
        fEnv = 0.0;
        fGain = 0.0;
        fHoldN = 0.0;
        fLpL = fLpR = 0.0;
        fLaSize = int(std::floor(0.01 * sr)) + 2;
        fLa.assign(size_t(2 * fLaSize), 0.0);
        fLaPos = 0;
    }

    // @slider
    void setParams(double thresholdDb, double attackMs, double releaseMs, double scHpf, double rangeDb,
                   double holdMs = 0.0, double lookaheadMs = 0.0, bool external = false, double scLpf = 20000.0,
                   bool flip = false)
    {
        fHoldS = std::floor(holdMs * fSr / 1000.0 + 0.5);
        fLaN = int(std::floor(lookaheadMs * fSr / 1000.0 + 0.5));
        fExt = external;
        fLpOn = scLpf < 20000.0;
        if (fLpOn)
            fLpCoef = 1.0 - std::exp(-2.0 * 3.141592653589793 * scLpf / fSr);
        fFlip = flip;
        fScHpf = scHpf;
        fRangeLin = rangeDb <= -80.0 ? 0.0 : std::pow(10.0, rangeDb / 20.0);
        fThresholdLin = std::pow(10.0, thresholdDb / 20.0);
        coefs(attackMs, releaseMs);
        if (fScHpf > 0.0)
            fLpfCoef = 1.0 - std::exp(-2.0 * 3.14159265358979 * fScHpf / fSr);
    }

    // @sample
    // kl / kr: the key (JSFX pins 3/4), used when External
    void processSample(double& l, double& r, double kl = 0.0, double kr = 0.0) noexcept
    {
        if (!fExt)
        {
            kl = l;
            kr = r;
        }
        double il, ir;
        if (fScHpf > 0.0)
        {
            fLpfL = fLpfL + fLpfCoef * (kl - fLpfL);
            fLpfR = fLpfR + fLpfCoef * (kr - fLpfR);
            il = kl - fLpfL;
            ir = kr - fLpfR;
        }
        else
        {
            il = kl;
            ir = kr;
        }
        if (fLpOn)
        {
            fLpL = fLpL + fLpCoef * (il - fLpL);
            fLpR = fLpR + fLpCoef * (ir - fLpR);
            il = fLpL;
            ir = fLpR;
        }
        const double al = il < 0.0 ? -il : il;
        const double ar = ir < 0.0 ? -ir : ir;
        const double dl = fEnvL * fPkDecay, dr = fEnvR * fPkDecay;
        fEnvL = al > dl ? al : dl;
        fEnvR = ar > dr ? ar : dr;
        fEnv = fEnvL > fEnvR ? fEnvL : fEnvR;
        bool trig = fEnv > fThresholdLin;
        if (fHoldS > 0.0)
        {
            if (trig)
                fHoldN = fHoldS;
            else if (fHoldN > 0.0)
            {
                fHoldN -= 1.0;
                trig = true;
            }
        }
        if (trig)
        {
            if (fGateLevel < 1.0)
                fGateLevel = fGateLevel + fAttackCoef * (1.0 - fGateLevel);
        }
        else if (fGateLevel > 0.0)
            fGateLevel = fGateLevel + fReleaseCoef * (0.0 - fGateLevel);
        fGain = fRangeLin + (1.0 - fRangeLin) * (fFlip ? 1.0 - fGateLevel : fGateLevel);
        fLa[size_t(fLaPos)] = l;
        fLa[size_t(fLaSize + fLaPos)] = r;
        if (fLaN > 0)
        {
            int k = fLaPos - fLaN;
            if (k < 0)
                k += fLaSize;
            l = fLa[size_t(k)];
            r = fLa[size_t(fLaSize + k)];
        }
        if (++fLaPos >= fLaSize)
            fLaPos = 0;
        l = l * fGain;
        r = r * fGain;
    }

    // the JSFX's pdc_delay: the lookahead in samples
    int latency() const noexcept { return fLaN; }

    double detector() const noexcept { return fEnv; }
    double gain() const noexcept { return fGain; }

private:
    void coefs(double attackMs, double releaseMs)
    {
        const double at = fSr * attackMs / 1000.0, rt = fSr * releaseMs / 1000.0;
        fAttackCoef = 1.0 - std::exp(-1.0 / (at > 1.0 ? at : 1.0));
        fReleaseCoef = 1.0 - std::exp(-1.0 / (rt > 1.0 ? rt : 1.0));
    }

    bool fExt = false, fLpOn = false, fFlip = false;
    double fHoldS = 0.0, fHoldN = 0.0, fLpCoef = 0.0, fLpL = 0.0, fLpR = 0.0;
    std::vector<double> fLa;
    int fLaSize = 2, fLaN = 0, fLaPos = 0;
    double fSr = 48000.0, fPkDecay = 0.0, fScHpf = 0.0, fRangeLin = 0.0, fThresholdLin = 0.01;
    double fAttackCoef = 0.0, fReleaseCoef = 0.0, fLpfCoef = 0.0;
    double fGateLevel = 0.0, fEnvL = 0.0, fEnvR = 0.0, fLpfL = 0.0, fLpfR = 0.0, fEnv = 0.0, fGain = 0.0;
};

} // namespace nullgate
