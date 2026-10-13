/*
 * NULL JSFX - Delay, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/delay.jsfx, computed in double like EEL2: a stereo delay line
 * (up to 2.1 s) with feedback (up to 95%) and a dry/wet mix. With Tempo Sync the time is a
 * 1/4, 1/8, 1/16 or 1/32 note at the host tempo, re-read every block as the JSFX's @block does.
 * Mode Ping-Pong feeds the mono input to the left line and cross-feeds the lines. Inside the
 * feedback loop each repeat passes a one-pole Low Cut and High Cut (off at 20 Hz / 20 kHz) and a
 * soft saturation (Drive, off at 0%).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulldelay {

// the delay time in ms for the settings (the JSFX's update_time)
inline double delayMs(double timeMs, bool sync, int subdivision, double tempo) noexcept
{
    if (sync && tempo > 0.0)
        return 60000.0 / tempo * (subdivision == 0 ? 1.0 : subdivision == 1 ? 0.5 : subdivision == 2 ? 0.25 : 0.125);
    return timeMs;
}

// soft saturation: a tanh-like curve, slope 1 at 0, flat at |x| = 3 (the JSFX's dl_sat)
inline double sat(double x) noexcept
{
    x = std::max(-3.0, std::min(3.0, x));
    return x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
}

class DelayProcessor {
public:
    // @init: buffers for 2.1 s, cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 2.1));
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fDelay = 1;
        fLcL = fLcR = fHcL = fHcR = 0.0;
        updateTime();
    }

    // @slider
    void setParams(double timeMs, double feedbackPct, double mixPct, bool sync, int subdivision, bool pingPong,
                   double lowCutHz, double highCutHz, double drivePct)
    {
        fTimeMs = timeMs;
        fSubdivision = subdivision;
        fFeedback = feedbackPct / 100.0 * 0.95;
        fMix = mixPct / 100.0;
        fSync = sync;
        fPing = pingPong;
        fLcOn = lowCutHz > 20.0;
        fLcA = 1.0 - std::exp(-2.0 * 3.141592653589793 * lowCutHz / fSr);
        fHcOn = highCutHz < 20000.0;
        fHcA = 1.0 - std::exp(-2.0 * 3.141592653589793 * highCutHz / fSr);
        fDrvOn = drivePct > 0.0;
        fDrvG = 1.0 + drivePct / 100.0 * 3.0;
        updateTime();
    }

    double delayMsNow() const noexcept { return fDelayMs; }
    double feedback() const noexcept { return fFeedback; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo)
    {
        fTempo = tempo;
        if (fSync)
            updateTime(); // @block
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            int r = fW - fDelay;
            if (r < 0)
                r += fSize;
            double el = fL[size_t(r)], er = fR[size_t(r)];
            // inside the feedback loop: every repeat passes the filters and the drive again
            if (fLcOn)
            {
                fLcL += fLcA * (el - fLcL);
                el -= fLcL;
                fLcR += fLcA * (er - fLcR);
                er -= fLcR;
            }
            else
                fLcL = fLcR = 0.0;
            if (fHcOn)
            {
                fHcL += fHcA * (el - fHcL);
                el = fHcL;
                fHcR += fHcA * (er - fHcR);
                er = fHcR;
            }
            else
            {
                fHcL = el;
                fHcR = er;
            }
            if (fDrvOn)
            {
                el = sat(el * fDrvG) / fDrvG;
                er = sat(er * fDrvG) / fDrvG;
            }
            if (fPing)
            {
                fL[size_t(fW)] = (xl + xr) * 0.5 + er * fFeedback;
                fR[size_t(fW)] = el * fFeedback;
            }
            else
            {
                fL[size_t(fW)] = xl + el * fFeedback;
                fR[size_t(fW)] = xr + er * fFeedback;
            }
            if (++fW >= fSize)
                fW = 0;
            outL[i] = static_cast<T>(xl * (1.0 - fMix) + el * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + er * fMix);
        }
    }

private:
    void updateTime()
    {
        fDelayMs = delayMs(fTimeMs, fSync, fSubdivision, fTempo);
        double d = std::floor(fDelayMs * fSr / 1000.0 + 0.5);
        d = std::max(1.0, std::min(double(fSize - 1), d));
        fDelay = int(d);
    }

    double fSr = 48000.0, fTempo = 120.0;
    std::vector<double> fL, fR;
    int fSize = 1, fW = 0, fDelay = 1, fSubdivision = 0;
    double fTimeMs = 250.0, fDelayMs = 250.0, fFeedback = 0.0, fMix = 0.3;
    double fLcA = 0.0, fHcA = 0.0, fDrvG = 1.0, fLcL = 0.0, fLcR = 0.0, fHcL = 0.0, fHcR = 0.0;
    bool fSync = false, fPing = false, fLcOn = false, fHcOn = false, fDrvOn = false;
};

} // namespace nulldelay
