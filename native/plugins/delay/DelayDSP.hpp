/*
 * NULL JSFX - Delay, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/delay.jsfx, computed in double like EEL2: a stereo delay line
 * (up to 2.1 s) with feedback (up to 95%) and a dry/wet mix. With Tempo Sync the time is a
 * 1/4, 1/8, 1/16 or 1/32 note at the host tempo, re-read every block as the JSFX's @block does.
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
        updateTime();
    }

    // @slider
    void setParams(double timeMs, double feedbackPct, double mixPct, bool sync, int subdivision)
    {
        fTimeMs = timeMs;
        fSubdivision = subdivision;
        fFeedback = feedbackPct / 100.0 * 0.95;
        fMix = mixPct / 100.0;
        fSync = sync;
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
            const double el = fL[size_t(r)], er = fR[size_t(r)];
            fL[size_t(fW)] = xl + el * fFeedback;
            fR[size_t(fW)] = xr + er * fFeedback;
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
    bool fSync = false;
};

} // namespace nulldelay
