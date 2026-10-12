/*
 * NULL JSFX - Maximizer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/maximizer.jsfx, computed in double like EEL2: Input Gain into
 * a look-ahead limiter holding the peaks at Ceiling (one gain for both channels with Stereo
 * Link, else one per channel), then an Output Gain trim (cut only). Last, optional TPDF dither
 * to 16 or 24 bits (exact Park-Miller steps in double, as the JSFX), with first-order
 * error-feedback noise shaping.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace nullmaximizer {

class MaximizerProcessor {
public:
    // @init: the look-ahead buffers (6 ms + 2), cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fMaxLook = int(std::ceil(sr * 0.006)) + 2;
        fL.assign(size_t(fMaxLook), 0.0);
        fR.assign(size_t(fMaxLook), 0.0);
        fW = 0;
        fHvL = fHvR = 0.0;
        fHcL = fHcR = 0.0;
        fGL = fGR = 1.0;
        fSeed = 12345.0;
        fEL = fER = 0.0;
    }

    // @slider
    void setParams(double inputDb, double ceilingDb, double releaseMs, double lookaheadMs, double outputDb, bool link, int dither,
                   bool noiseShaping)
    {
        fInGain = std::pow(10.0, inputDb / 20.0);
        fCeiling = std::pow(10.0, ceilingDb / 20.0);
        fRel = std::exp(-1.0 / (releaseMs * fSr / 1000.0));
        fLook = int(std::floor(lookaheadMs * fSr / 1000.0 + 0.5));
        fAtt = fLook < 1 ? 1.0 : 1.0 - std::exp(-5.0 / double(fLook));
        fOutGain = std::pow(10.0, std::min(outputDb, 0.0) / 20.0);
        fLink = link;
        fDither = dither;
        fQ = dither == 1 ? 32768.0 : 8388608.0;
        fShape = noiseShaping;
    }

    int latency() const noexcept { return fLook; } // pdc_delay
    double gain() const noexcept { return std::min(fGL, fGR); }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        const double il = l * fInGain, ir = r * fInGain;
        if (fLink)
        {
            detect(std::max(std::fabs(il), std::fabs(ir)), fHvL, fHcL, fGL);
            fGR = fGL;
        }
        else
        {
            detect(std::fabs(il), fHvL, fHcL, fGL);
            detect(std::fabs(ir), fHvR, fHcR, fGR);
        }
        fL[size_t(fW)] = il;
        fR[size_t(fW)] = ir;
        int rp = fW - fLook;
        if (rp < 0)
            rp += fMaxLook;
        if (++fW >= fMaxLook)
            fW = 0;
        l = std::max(-fCeiling, std::min(fCeiling, fL[size_t(rp)] * fGL)) * fOutGain;
        r = std::max(-fCeiling, std::min(fCeiling, fR[size_t(rp)] * fGR)) * fOutGain;
        if (fDither != 0)
        {
            l = quantise(l, fEL);
            r = quantise(r, fER);
        }
    }

private:
    // one exact Park-Miller step, a uniform value in -1..1 (as riser.jsfx)
    double rnd() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 1073741824.0 - 1.0;
    }

    // TPDF dither (half the sum of two uniform values = +-1 LSB triangular), rounding to the
    // bit depth; with Noise Shaping the last error is subtracted first (1 - z^-1)
    double quantise(double x, double& err) noexcept
    {
        const double n1 = rnd();
        const double n2 = rnd();
        const double v = fShape ? x - err : x;
        const double q = std::floor(v * fQ + (n1 + n2) * 0.5 + 0.5) / fQ;
        err = fShape ? q - v : 0.0;
        return q;
    }

    void detect(double peak, double& hv, double& hc, double& g) const noexcept
    {
        if (peak >= hv)
        {
            hv = peak;
            hc = fLook;
        }
        else if (hc > 0.0)
            hc -= 1.0;
        else
            hv = std::max(peak, hv * fRel);
        const double target = hv > fCeiling ? fCeiling / hv : 1.0;
        g = target < g ? g + (target - g) * fAtt : target + (g - target) * fRel;
    }

    double fSr = 48000.0;
    std::vector<double> fL, fR;
    int fMaxLook = 1, fW = 0, fLook = 0;
    double fHvL = 0.0, fHvR = 0.0, fHcL = 0.0, fHcR = 0.0, fGL = 1.0, fGR = 1.0;
    double fInGain = 1.0, fCeiling = 1.0, fRel = 0.0, fAtt = 1.0, fOutGain = 1.0;
    double fSeed = 12345.0, fQ = 32768.0, fEL = 0.0, fER = 0.0;
    int fDither = 0;
    bool fLink = true, fShape = false;
};

} // namespace nullmaximizer
