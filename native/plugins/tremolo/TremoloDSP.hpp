/*
 * NULL JSFX - Tremolo, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/tremolo.jsfx, computed in double like EEL2: an LFO (Sine,
 * Triangle, Square with ~2 ms edges, Saw Up/Down with a ~2 ms turn-around) moves the gain
 * between 1 (LFO peak) and 1 - Depth (trough), blended with the dry signal (Mix). Tempo Sync
 * takes the rate from the host tempo and, while playing, the phase from its beat position
 * (re-read every block like the JSFX's @block); Stereo Phase offsets the right channel's LFO;
 * Harmonic splits each channel at 800 Hz (two one-pole low-passes, the highs the rest) and
 * moves the highs with the inverted LFO.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulltremolo {

static constexpr double kTwoPi = 6.28318530717958; // the JSFX's literal, not 2 * M_PI

// the output level (dry and wet together) at LFO value lfo (-1..1)
inline double levelAt(double depthPct, double mixPct, double lfo) noexcept
{
    return 1.0 - mixPct / 100.0 * depthPct / 100.0 * (0.5 - 0.5 * lfo);
}

// beats per LFO cycle of a Tempo Sync option (1 = 1/1 ... 11 = 1/8D)
inline double syncBeats(int sync) noexcept
{
    switch (sync)
    {
    case 1: return 4.0;
    case 2: return 2.0;
    case 3: return 1.0;
    case 4: return 0.5;
    case 5: return 0.25;
    case 6: return 0.125;
    case 7: return 2.0 / 3.0;
    case 8: return 1.0 / 3.0;
    case 9: return 1.0 / 6.0;
    case 10: return 1.5;
    case 11: return 0.75;
    default: return 1.0;
    }
}

// the JSFX's tr_edges(): Square's clip gain and the saws' turn-around (cycles) for a rate in Hz
struct Edges {
    double sqG = 1.0, sawW = 0.01;
};
inline Edges edgesFor(double hz) noexcept
{
    Edges e;
    e.sqG = std::max(1.0, 1.0 / std::max(0.000001, kTwoPi * hz * 0.002));
    e.sawW = std::max(0.0001, std::min(0.5, hz * 0.002));
    return e;
}

// the JSFX's tr_lfo() (shape 1..4) and sin() (shape 0) at phase ph (radians)
inline double lfoValue(int shape, double ph, const Edges& e) noexcept
{
    if (shape == 0)
        return std::sin(ph);
    if (shape == 1)
    {
        double u = ph / kTwoPi;
        u -= std::floor(u);
        return u < 0.25 ? 4.0 * u : (u < 0.75 ? 2.0 - 4.0 * u : 4.0 * u - 4.0);
    }
    if (shape == 2)
        return std::max(-1.0, std::min(1.0, std::sin(ph) * e.sqG));
    double u = ph / kTwoPi + 0.5;
    u -= std::floor(u);
    const double sw = e.sawW;
    u = u < 1.0 - sw ? -1.0 + 2.0 * u / (1.0 - sw) : 1.0 - 2.0 * (u - (1.0 - sw)) / sw;
    return shape == 4 ? -u : u;
}

class TremoloProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPhase = 0.0;
        fXl1 = fXl2 = fXr1 = fXr2 = 0.0;
    }

    // @slider
    void setParams(double rateHz, double depthPct, double mixPct, int shape = 0, int sync = 0, double stereoDeg = 0.0,
                   bool harmonic = false)
    {
        fDepth = depthPct / 100.0;
        fMix = mixPct / 100.0;
        fPhaseInc = kTwoPi * rateHz / fSr;
        fShape = shape;
        fSync = sync;
        fBeats = syncBeats(sync);
        fStOn = stereoDeg > 0.0;
        fStOff = stereoDeg / 360.0 * kTwoPi;
        fHarm = harmonic;
        fXoA = 1.0 - std::exp(-kTwoPi * 800.0 / fSr);
        fE = edgesFor(fPhaseInc * fSr / kTwoPi);
    }

    double phase() const noexcept { return fPhase / kTwoPi; } // 0..1 of the cycle
    double rateHz() const noexcept { return fPhaseInc * fSr / kTwoPi; } // the rate in use

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo = 120.0,
                 bool playing = false, double beatPos = 0.0)
    {
        // @block
        if (fSync)
        {
            const double bpm = tempo > 0.0 ? tempo : 120.0;
            fPhaseInc = kTwoPi * (bpm / 60.0 / fBeats) / fSr;
            if (playing)
            {
                const double b = beatPos / fBeats;
                fPhase = kTwoPi * (b - std::floor(b));
            }
            fE = edgesFor(fPhaseInc * fSr / kTwoPi);
        }
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            if (fPhase >= kTwoPi)
                fPhase = fPhase - kTwoPi;
            const double lfo = lfoValue(fShape, fPhase, fE);
            double lfoR = 0.0;
            if (fStOn)
                lfoR = lfoValue(fShape, fPhase + fStOff, fE);
            fPhase = fPhase + fPhaseInc;
            const double a = 1.0 - fDepth * (0.5 - 0.5 * lfo);
            const double ar = fStOn ? 1.0 - fDepth * (0.5 - 0.5 * lfoR) : a;
            double pl, pr;
            if (fHarm)
            {
                fXl1 = fXl1 + fXoA * (xl - fXl1);
                fXl2 = fXl2 + fXoA * (fXl1 - fXl2);
                fXr1 = fXr1 + fXoA * (xr - fXr1);
                fXr2 = fXr2 + fXoA * (fXr1 - fXr2);
                pl = fXl2 * a + (xl - fXl2) * (1.0 - fDepth * (0.5 + 0.5 * lfo));
                pr = fXr2 * ar + (xr - fXr2) * (1.0 - fDepth * (0.5 + 0.5 * (fStOn ? lfoR : lfo)));
            }
            else
            {
                fXl1 = fXl2 = fXr1 = fXr2 = 0.0;
                pl = xl * a;
                pr = xr * ar;
            }
            outL[i] = static_cast<T>(xl * (1.0 - fMix) + pl * fMix);
            outR[i] = static_cast<T>(xr * (1.0 - fMix) + pr * fMix);
        }
    }

private:
    double fSr = 48000.0, fPhase = 0.0, fPhaseInc = 0.0, fDepth = 0.5, fMix = 0.4;
    int fShape = 0, fSync = 0;
    double fBeats = 1.0, fStOff = 0.0, fXoA = 0.0;
    bool fStOn = false, fHarm = false;
    double fXl1 = 0.0, fXl2 = 0.0, fXr1 = 0.0, fXr2 = 0.0;
    Edges fE {};
};

} // namespace nulltremolo
