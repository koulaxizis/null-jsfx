/*
 * NULL JSFX - Flanger, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/flanger.jsfx, computed in double like EEL2: a sine LFO sweeps
 * a stereo delay line between Manual and Manual + Depth (linear interpolation) with feedback
 * (up to 70%), blended with the dry signal (Mix). Stereo Phase offsets the right channel's LFO;
 * Tempo Sync sets one cycle to a note (locked to the beat position while playing); Thru Zero
 * delays the dry path by the sweep's midpoint (whole samples, the plugin's latency) and mixes
 * the swept path in opposite polarity.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullflanger {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI
static constexpr double kPi = 3.141592653589793;   // EEL2's $pi

// the delay time in ms at LFO value lfo (-1..1)
inline double delayMsAt(double depthMs, double lfo, double manualMs = 0.5) noexcept
{
    return manualMs + depthMs * 0.5 * (1.0 + lfo);
}

class FlangerProcessor {
public:
    // @init: 50 ms buffers (swept and dry), cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 0.05));
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fDryL.assign(size_t(fSize), 0.0);
        fDryR.assign(size_t(fSize), 0.0);
        fW = 0;
        fPhase = 0.0;
        fFbL = fFbR = 0.0;
    }

    // @slider (the values on the JSFX slider grid; sync = the Tempo Sync option, cycle its beats)
    void setParams(double rateHz, double depthMs, double feedbackPct, double mixPct, double manualMs = 0.5,
                   int sync = 0, double cycleBeats = 1.0, double phaseDeg = 0.0, bool thruZero = false)
    {
        fDepth = depthMs;
        fFb = feedbackPct / 100.0 * 0.7;
        fMix = mixPct / 100.0;
        fPhaseInc = kTwoPi * rateHz / fSr;
        fManual = manualMs;
        fSync = sync;
        fCyc = cycleBeats;
        fStOff = phaseDeg * kPi / 180.0;
        fTz = thruZero;
        fTzDelay = int(std::floor((manualMs + depthMs * 0.5) * fSr / 1000.0 + 0.5));
    }

    double phase() const noexcept { return fPhase / kTwoPi; } // 0..1 of the cycle
    double rateHz() const noexcept { return fPhaseInc * fSr / kTwoPi; }
    int thruZeroDelay() const noexcept { return fTzDelay; }   // the dry path's delay in samples
    int latency() const noexcept { return fTz ? fTzDelay : 0; } // the JSFX's pdc_delay

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
            double xl = inL[i], xr = inR[i];
            if (fPhase >= kTwoPi)
                fPhase = fPhase - kTwoPi;
            const double ph = fPhase;
            const double lfo = std::sin(fPhase);
            fPhase = fPhase + fPhaseInc;

            const double ds = (fManual + fDepth * 0.5 * (1.0 + lfo)) * fSr / 1000.0;
            double r = double(fW) - ds;
            r = r < 0.0 ? r + double(fSize) : r;
            double ri = std::floor(r), rf = r - ri;
            size_t i1 = size_t(ri), i2 = ri >= double(fSize - 1) ? 0 : size_t(ri) + 1;
            double dl = fL[i1] + (fL[i2] - fL[i1]) * rf;

            if (fStOff > 0.0)
            {
                // the right channel's own sweep
                const double md = fManual + fDepth * 0.5 * (1.0 + std::sin(ph + fStOff));
                r = double(fW) - md * fSr / 1000.0;
                r = r < 0.0 ? r + double(fSize) : r;
                ri = std::floor(r);
                rf = r - ri;
                i1 = size_t(ri);
                i2 = ri >= double(fSize - 1) ? 0 : size_t(ri) + 1;
            }
            double dr = fR[i1] + (fR[i2] - fR[i1]) * rf;

            fL[size_t(fW)] = xl + fFbL * fFb;
            fR[size_t(fW)] = xr + fFbR * fFb;
            fFbL = dl;
            fFbR = dr;

            fDryL[size_t(fW)] = xl;
            fDryR[size_t(fW)] = xr;
            if (fTz)
            {
                int rd = fW - fTzDelay;
                if (rd < 0)
                    rd += fSize;
                xl = fDryL[size_t(rd)];
                xr = fDryR[size_t(rd)];
                dl = -dl;
                dr = -dr;
            }

            if (++fW >= fSize)
                fW = 0;

            outL[i] = static_cast<T>(xl * (1.0 - fMix) + dl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + dr * fMix);
        }
    }

private:
    double fSr = 48000.0, fPhase = 0.0, fPhaseInc = 0.0, fDepth = 5.0, fFb = 0.35, fMix = 0.4;
    double fManual = 0.5, fCyc = 1.0, fStOff = 0.0;
    double fFbL = 0.0, fFbR = 0.0;
    int fSync = 0, fTzDelay = 0;
    bool fTz = false;
    std::vector<double> fL, fR, fDryL, fDryR;
    int fSize = 1, fW = 0;
};

} // namespace nullflanger
