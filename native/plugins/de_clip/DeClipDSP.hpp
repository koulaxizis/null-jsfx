/*
 * NULL JSFX - De-Clip, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/de_clip.jsfx, computed in double like EEL2: a run of at least
 * Min Length samples in a row at or above Clip Level (per channel) is redrawn with the cubic
 * through the two good samples on either side, kept on its side of the clip level and at most
 * 12 dB above it. The output runs 128 samples late (the plugin's latency), so runs up to 120
 * samples long are rebuilt.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulldeclip {

struct Params {
    double clipLevel = -0.3, minLength = 3.0, output = 0.0;
    bool delta = false;
};

class DeClipProcessor {
public:
    static constexpr int kRing = 256, kLook = 128;

    // @init
    void setSampleRate(double)
    {
        for (int i = 0; i < kRing; ++i)
            fXL[i] = fXR[i] = fYL[i] = fYR[i] = 0.0;
        fW = 0;
        fRuns = 0;
    }

    // @slider
    void setParams(const Params& p)
    {
        fThr = std::pow(10.0, p.clipLevel / 20.0);
        fMin = std::max(2.0, std::min(8.0, std::floor(p.minLength + 0.5)));
        fGain = std::pow(10.0, p.output / 20.0);
        fDelta = p.delta;
    }

    static constexpr int latency() noexcept { return kLook; }
    // runs rebuilt so far (both channels), for the display
    uint32_t runs() const noexcept { return fRuns; }
    // the input (mid) at the moment being output, for the display
    double lastInMid() const noexcept { return fInMid; }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        fXL[fW] = fYL[fW] = l;
        fXR[fW] = fYR[fW] = r;
        fW = (fW + 1) % kRing;
        fix(fXL, fYL);
        fix(fXR, fYR);
        const int o = (fW - 1 - kLook + kRing) % kRing;
        if (fDelta)
        {
            l = (fYL[o] - fXL[o]) * fGain;
            r = (fYR[o] - fXR[o]) * fGain;
        }
        else
        {
            l = fYL[o] * fGain;
            r = fYR[o] * fGain;
        }
        fInMid = (fXL[o] + fXR[o]) * 0.5;
    }

private:
    // dc_fix: a run that ended 3 samples ago, redrawn through nodes at -2, -1, m, m + 1
    void fix(const double* x, double* y) noexcept
    {
        const int c = (fW - 3 + kRing) % kRing;
        if (!(std::fabs(x[c]) >= fThr && std::fabs(x[(c + 1) % kRing]) < fThr))
            return;
        const double sg = x[c] > 0.0 ? 1.0 : -1.0;
        double m = 1.0;
        int s = c;
        for (;;)
        {
            const int k = (s - 1 + kRing) % kRing;
            if (!(m <= kLook - 8 && x[k] * sg >= fThr))
                break;
            s = k;
            m += 1.0;
        }
        if (!(m >= fMin && m <= kLook - 8))
            return;
        const double a0 = x[(s - 2 + kRing) % kRing], a1 = x[(s - 1 + kRing) % kRing];
        const double b0 = x[(c + 1) % kRing], b1 = x[(c + 2) % kRing];
        const double d0 = (m + 2.0) * (m + 3.0), d1 = (m + 1.0) * (m + 2.0);
        double t = 0.0;
        for (int i = 0; i < int(m); ++i)
        {
            double v = -(t + 1.0) * (t - m) * (t - m - 1.0) / d0 * a0
                       + (t + 2.0) * (t - m) * (t - m - 1.0) / d1 * a1
                       - (t + 2.0) * (t + 1.0) * (t - m - 1.0) / d1 * b0
                       + (t + 2.0) * (t + 1.0) * (t - m) / d0 * b1;
            v = sg * std::min(std::max(v * sg, fThr), fThr * 4.0);
            y[(s + i) % kRing] = v;
            t += 1.0;
        }
        ++fRuns;
    }

    double fXL[kRing] = {}, fXR[kRing] = {}, fYL[kRing] = {}, fYR[kRing] = {};
    int fW = 0;
    double fThr = 0.9660508789898133, fMin = 3.0, fGain = 1.0, fInMid = 0.0;
    bool fDelta = false;
    uint32_t fRuns = 0;
};

inline Params params(double clipLevel, double minLength, double output, double listen)
{
    Params p;
    p.clipLevel = clipLevel;
    p.minLength = minLength;
    p.output = output;
    p.delta = listen >= 0.5;
    return p;
}

} // namespace nulldeclip
