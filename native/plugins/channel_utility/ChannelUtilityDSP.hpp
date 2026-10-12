/*
 * NULL JSFX - Channel Utility, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/channel_utility.jsfx, computed in double like EEL2. In order:
 * input pad (0, -6, -12 dB) and gain, polarity per channel, a one-pole DC blocker with a
 * dry/wet mix, balance per channel, solo left or right, mono check, output gain and a hard
 * clip at 0.99.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullchannelutility {

struct Params {
    int pad = 0;           // 0, 1 (-6 dB), 2 (-12 dB)
    double inGainDb = 0.0;
    bool invL = false, invR = false;
    double dcFreq = 20.0, dcMixPct = 100.0;
    double balLDb = 0.0, balRDb = 0.0;
    bool soloL = false, soloR = false, mono = false;
    double outGainDb = 0.0;
    bool limit = false;
};

class ChannelUtilityProcessor {
public:
    static constexpr double kDb2Lin = 0.11512925464970228420;

    // @init (state cleared) and the srate-dependent part of @slider
    void setSampleRate(double sr)
    {
        fSr = sr;
        fXl = fXr = fYl = fYr = 0.0;
        setParams(fP);
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        const double padDb = p.pad == 1 ? -6.0 : (p.pad == 2 ? -12.0 : 0.0);
        fPad = std::exp(padDb * kDb2Lin);
        fIn = std::exp(p.inGainDb * kDb2Lin);
        fDcMix = p.dcMixPct / 100.0;
        fDcAlpha = 1.0 / (1.0 + 3.14159265358979 * p.dcFreq / fSr);
        fBalL = std::exp(p.balLDb * kDb2Lin);
        fBalR = std::exp(p.balRDb * kDb2Lin);
        fOut = std::exp(p.outGainDb * kDb2Lin);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = double(inL[i]) * fPad * fIn;
            double r = double(inR[i]) * fPad * fIn;
            if (fP.invL)
                l = -l;
            if (fP.invR)
                r = -r;
            const double bl = fDcAlpha * (fYl + l - fXl);
            const double br = fDcAlpha * (fYr + r - fXr);
            fXl = l; fXr = r;
            fYl = bl; fYr = br;
            l = (1.0 - fDcMix) * l + fDcMix * bl;
            r = (1.0 - fDcMix) * r + fDcMix * br;
            l *= fBalL;
            r *= fBalR;
            if (fP.soloL)
                r = 0.0;
            else if (fP.soloR)
                l = 0.0;
            if (fP.mono)
            {
                l = (l + r) / 2.0;
                r = l;
            }
            l *= fOut;
            r *= fOut;
            if (fP.limit)
            {
                if (std::fabs(l) > 0.99)
                    l = (l > 0.0 ? 1.0 : -1.0) * 0.99;
                if (std::fabs(r) > 0.99)
                    r = (r > 0.0 ? 1.0 : -1.0) * 0.99;
            }
            outL[i] = static_cast<T>(l);
            outR[i] = static_cast<T>(r);
        }
    }

private:
    Params fP;
    double fSr = 48000.0;
    double fPad = 1.0, fIn = 1.0, fDcMix = 1.0, fDcAlpha = 1.0, fBalL = 1.0, fBalR = 1.0, fOut = 1.0;
    double fXl = 0.0, fXr = 0.0, fYl = 0.0, fYr = 0.0;
};

} // namespace nullchannelutility
