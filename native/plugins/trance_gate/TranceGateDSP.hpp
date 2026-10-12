/*
 * NULL JSFX - Trance Gate, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/trance_gate.jsfx, computed in double like EEL2: a 16-step
 * gate. Sync: one step = Step Length at the host tempo, locked to the host's beat position
 * while the transport plays (re-read every block, as the JSFX's @block does), free-running
 * at the tempo when stopped. Free: Free Rate steps per second. The gain ramps linearly
 * toward 1 (on steps) or 1 - Depth (off steps) over Attack / Release.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulltrance {

struct Params {
    bool steps[16] = {};
    bool freeMode = false;
    int stepLength = 2;    // 0..3: 1/4, 1/8, 1/16, 1/32
    double freeRate = 8.0; // steps per second
    double attackMs = 5.0, releaseMs = 40.0, depthPct = 100.0;
};

// The display (trance_gate_gfx.jsfx draws the same): the gain over the 16 steps as the
// attack / release ramps shape it, as a polyline of up to 3 points per step. step = the
// step duration in seconds. Writes up to 49 (t in steps, gain) points, returns the count.
inline int displayEnvelope(const Params& p, double step, double* ts, double* gs) noexcept
{
    const double depth = p.depthPct / 100.0;
    const double up = step * 1000.0 / p.attackMs, down = step * 1000.0 / p.releaseMs; // gain change per step
    double g = 1.0;
    int n = 0;
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k < 16; ++k)
        {
            const double target = 1.0 - depth * (p.steps[k] ? 0.0 : 1.0);
            if (pass == 1)
            {
                ts[n] = k; gs[n] = g; ++n;
            }
            const double rate = target > g ? up : down;
            const double need = std::fabs(target - g) / rate; // steps to reach the target
            if (need < 1.0)
            {
                if (pass == 1)
                {
                    ts[n] = k + need; gs[n] = target; ++n;
                }
                g = target;
            }
            else
                g += target > g ? rate : -rate;
        }
    ts[n] = 16.0; gs[n] = g; ++n;
    return n;
}

class TranceGateProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fStepPos = 0.0;
        fGain = 1.0;
        fCur = 0;
        updateParams();
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    int currentStep() const noexcept { return fCur; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo, bool playing,
                 double beatPos)
    {
        // @block
        const double bpm = tempo > 0.0 ? tempo : 120.0;
        if (fP.freeMode)
            fStepInc = fP.freeRate / fSr;
        else
        {
            fStepInc = bpm / 60.0 * fStepsPerBeat / fSr;
            if (playing)
                fStepPos = beatPos * fStepsPerBeat;
        }
        for (uint32_t i = 0; i < frames; ++i)
        {
            fStepPos += fStepInc;
            if (fStepPos >= 16.0)
                fStepPos -= 16.0;
            const double s = std::floor(fStepPos);
            const double cur = s - 16.0 * std::floor(s / 16.0);
            fCur = int(cur + 0.0001);
            const double target = 1.0 - fDepth * (1.0 - (fP.steps[fCur & 15] ? 1.0 : 0.0));
            if (target > fGain)
            {
                fGain += fAttack;
                if (fGain > target)
                    fGain = target;
            }
            else
            {
                fGain -= fRelease;
                if (fGain < target)
                    fGain = target;
            }
            outL[i] = static_cast<T>(inL[i] * fGain);
            outR[i] = static_cast<T>(inR[i] * fGain);
        }
    }

private:
    void updateParams()
    {
        fStepsPerBeat = std::pow(2.0, double(fP.stepLength));
        fAttack = 1.0 / std::fmax(1.0, fSr * fP.attackMs * 0.001);
        fRelease = 1.0 / std::fmax(1.0, fSr * fP.releaseMs * 0.001);
        fDepth = fP.depthPct / 100.0;
    }

    Params fP;
    double fSr = 48000.0, fStepPos = 0.0, fStepInc = 0.0, fGain = 1.0;
    double fStepsPerBeat = 4.0, fAttack = 0.0, fRelease = 0.0, fDepth = 1.0;
    int fCur = 0;
};

} // namespace nulltrance
