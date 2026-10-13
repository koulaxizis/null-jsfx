/*
 * NULL JSFX - Tape Stop, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/tape_stop.jsfx, computed in double like EEL2. Idle: the audio
 * passes untouched. Trigger: playback of a 12 s buffer slows to a stop over Stop Time (speed
 * curved like a tape motor), spins back up over Start Time and fades back to the live signal.
 *
 * The JSFX resets its Trigger slider to Idle when the spin-up ends. A plugin cannot move its own
 * parameter, so the processor keeps that slider as a latch: set from the parameter when it
 * changes, cleared at the end of the spin-up (the interface then sets the parameter back).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulltapestop {

// the tape speed at x (0..1 across stop + start), as in @sample (ts_rate in the GFX)
inline double rateAt(double x, double stopSamples, double startSamples) noexcept
{
    const double t = x * (stopSamples + startSamples);
    if (t < stopSamples)
    {
        const double r = 1 - t / stopSamples;
        return r * r;
    }
    return std::sqrt(std::min(1.0, (t - stopSamples) / startSamples));
}

class TapeStopProcessor {
public:
    // @init (the trigger latch, being the slider, survives it)
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 12));
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fWrite = 0;
        fRead = 0;
        fRate = 1;
        fState = 0;
        fTimer = 0;
        fWet = 0;
        fFade = 1 / (0.01 * sr);
    }

    // the Trigger slider changed
    void setTrigger(double v) noexcept { fTrigger = v; }
    double trigger() const noexcept { return fTrigger; }

    // @slider
    void setParams(double stopMs, double startMs)
    {
        // EEL2 evaluates srate * slider2 / 1000 as srate * (slider2 / 1000)
        fStop = std::max(1.0, fSr * (stopMs / 1000));
        fStart = std::max(1.0, fSr * (startMs / 1000));
        if (fTrigger >= 1 && fState == 0)
        {
            fState = 1;
            fTimer = 0;
            fRead = double(fWrite - 1);
            if (fRead < 0)
                fRead += fSize;
        }
    }

    int state() const noexcept { return fState; }
    double stopSamples() const noexcept { return fStop; }
    double startSamples() const noexcept { return fStart; }
    // where playback is across stop + start (0..1), or -1 when idle
    double position() const noexcept
    {
        if (fState == 0)
            return -1.0;
        return ((fState == 1 ? 0.0 : fStop) + fTimer) / (fStop + fStart);
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            fL[size_t(fWrite)] = xl;
            fR[size_t(fWrite)] = xr;
            if (++fWrite >= fSize)
                fWrite = 0;

            if (fState == 1)
            {
                // slowing down (curved like a real tape motor)
                fRate = 1 - fTimer / fStop;
                fRate = fRate * fRate;
                fTimer += 1;
                if (fTimer >= fStop)
                {
                    fState = 2; fTimer = 0; fRate = 0;
                }
            }
            else if (fState == 2)
            {
                // spinning back up
                fRate = fTimer / fStart;
                fRate = std::sqrt(fRate);
                fTimer += 1;
                if (fTimer >= fStart)
                {
                    fState = 0;
                    fRate = 1;
                    fTrigger = 0;
                }
            }

            // the wet signal fades in at the trigger and back out after the spin-up
            fWet = fState > 0 ? std::min(1.0, fWet + fFade) : std::max(0.0, fWet - fFade);

            double yl = xl, yr = xr;
            if (fWet > 0)
            {
                fRead += fRate;
                if (fRead >= fSize)
                    fRead -= fSize;
                const double fi = std::floor(fRead);
                const double fr = fRead - fi;
                const int ri = int(fi);
                int r2 = ri + 1;
                if (r2 >= fSize)
                    r2 = 0;
                const double tl = fL[size_t(ri)] + (fL[size_t(r2)] - fL[size_t(ri)]) * fr;
                const double tr = fR[size_t(ri)] + (fR[size_t(r2)] - fR[size_t(ri)]) * fr;
                // tape output also dies away as the motor stops
                const double lvl = std::min(1.0, fRate * 8);
                yl = xl * (1 - fWet) + tl * lvl * fWet;
                yr = xr * (1 - fWet) + tr * lvl * fWet;
            }
            outL[n] = static_cast<T>(yl);
            outR[n] = static_cast<T>(yr);
        }
    }

private:
    double fSr = 48000.0;
    std::vector<double> fL, fR;
    int fSize = 1, fWrite = 0, fState = 0;
    double fRead = 0, fRate = 1, fTimer = 0, fWet = 0, fFade = 0.002;
    double fStop = 24000, fStart = 24000, fTrigger = 0;
};

} // namespace nulltapestop
