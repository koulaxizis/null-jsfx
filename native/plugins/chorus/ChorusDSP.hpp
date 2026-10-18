/*
 * NULL JSFX - Chorus, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/chorus.jsfx, computed in double like EEL2: sine LFOs sweep
 * taps on a stereo delay line between 5 ms and 5 ms + Depth (linear interpolation), blended
 * with the dry signal (Mix). Classic: Voices taps spread evenly in LFO phase, the right
 * channel's shifted by Stereo Spread. Dimension: two taps swept in opposite phase by a
 * triangle, spread as mid/side. Feedback into the lines, a one-pole High Cut on the wet,
 * and Tempo Sync (one LFO cycle = a note, locked to the beat position while playing).
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullchorus {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI
static constexpr double kPi = 3.141592653589793;   // EEL2's $pi

// the delay time in ms at LFO value lfo (-1..1)
inline double delayMsAt(double depthMs, double lfo) noexcept
{
    const double mod = depthMs * 0.5;
    return 5.0 + mod + lfo * mod;
}

// the Dimension triangle (-1 at phase 0, +1 at half a cycle) at phase ph (radians, 0..2 pi)
inline double triAt(double ph) noexcept { return 1.0 - 2.0 * std::fabs(ph / kTwoPi * 2.0 - 1.0); }

class ChorusProcessor {
public:
    // @init: 100 ms buffers, cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 0.1));
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fPhase = 0.0;
        fHcL = fHcR = 0.0;
    }

    // @slider (the values on the JSFX slider grid; sync = the Tempo Sync option, cycle its beats)
    void setParams(double rateHz, double depthMs, double mixPct, double voices, double spreadPct,
                   double feedbackPct, int sync, double cycleBeats, double highCutHz, bool dimension)
    {
        fDepth = depthMs;
        fMix = mixPct / 100.0;
        fPhaseInc = kTwoPi * rateHz / fSr;
        fNv = std::fmax(1.0, std::fmin(4.0, std::floor(voices + 0.5)));
        fVOff = kTwoPi / fNv;
        fSpread = spreadPct;
        fSpOff = spreadPct / 100.0 * kPi / fNv;
        fFb = feedbackPct / 100.0;
        fSync = sync;
        fCyc = cycleBeats;
        fHcOn = highCutHz < 20000.0;
        fHcA = 1.0 - std::exp(-2.0 * kPi * highCutHz / fSr);
        fDim = dimension;
    }

    double phase() const noexcept { return fPhase / kTwoPi; } // 0..1 of the cycle
    double rateHz() const noexcept { return fPhaseInc * fSr / kTwoPi; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, bool playing = false,
                 double beatPos = 0.0, double tempo = 120.0)
    {
        // @block
        if (fSync > 0)
        {
            const double bpm = tempo > 0.0 ? tempo : 120.0;
            fPhaseInc = kTwoPi * bpm / 60.0 / fCyc / fSr;
            if (playing)
                fPhase = kTwoPi * (beatPos / fCyc - std::floor(beatPos / fCyc));
        }

        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            if (fPhase >= kTwoPi)
                fPhase = fPhase - kTwoPi;
            const double ph = fPhase;
            const double lfo = std::sin(fPhase);
            fPhase = fPhase + fPhaseInc;

            fMod = fDepth * 0.5;
            fBase = 5.0 + fMod;

            double dl, dr;
            if (fDim)
            {
                const double tri = triAt(ph);
                dl = tap(fL, tri);
                dr = tap(fR, -tri);
            }
            else
            {
                dl = tap(fL, lfo);
                dr = fSpOff > 0.0 ? tap(fR, std::sin(ph + fSpOff)) : tap(fR, lfo);
                if (fNv > 1.0)
                {
                    double v = 1.0;
                    for (int k = 0; k < int(fNv) - 1; ++k)
                    {
                        const double p = ph + v * fVOff;
                        dl += tap(fL, std::sin(p));
                        dr += tap(fR, std::sin(p + fSpOff));
                        v += 1.0;
                    }
                    dl /= fNv;
                    dr /= fNv;
                }
            }

            if (fHcOn)
            {
                fHcL += fHcA * (dl - fHcL);
                dl = fHcL;
                fHcR += fHcA * (dr - fHcR);
                dr = fHcR;
            }

            if (fFb > 0.0)
            {
                fL[size_t(fW)] = xl + dl * fFb;
                fR[size_t(fW)] = xr + dr * fFb;
            }
            else
            {
                fL[size_t(fW)] = xl;
                fR[size_t(fW)] = xr;
            }
            if (++fW >= fSize)
                fW = 0;

            if (fDim)
            {
                const double m = (dl + dr) * 0.5;
                const double s = (dl - dr) * 0.5 * (0.5 + fSpread / 200.0);
                dl = m + s;
                dr = m - s;
            }

            outL[i] = static_cast<T>(xl * (1.0 - fMix) + dl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + dr * fMix);
        }
    }

private:
    // ch_tap: buf read at lfo (-1..1) along the sweep, linearly interpolated
    double tap(const std::vector<double>& buf, double lfo) const noexcept
    {
        double r = double(fW) - (fBase + lfo * fMod) * fSr / 1000.0;
        r = r < 0.0 ? r + double(fSize) : r;
        const double ri = std::floor(r), rf = r - ri;
        const size_t i1 = size_t(ri), i2 = ri >= double(fSize - 1) ? 0 : size_t(ri) + 1;
        const double a = buf[i1], b = buf[i2];
        return a + (b - a) * rf;
    }

    double fSr = 48000.0, fPhase = 0.0, fPhaseInc = 0.0, fDepth = 3.0, fMix = 0.5;
    double fMod = 1.5, fBase = 6.5;
    double fNv = 1.0, fVOff = kTwoPi, fSpread = 0.0, fSpOff = 0.0, fFb = 0.0, fCyc = 1.0;
    double fHcA = 1.0, fHcL = 0.0, fHcR = 0.0;
    bool fHcOn = false, fDim = false;
    int fSync = 0;
    std::vector<double> fL, fR;
    int fSize = 1, fW = 0;
};

} // namespace nullchorus
