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
 * Probability and Seed (appended in 2.4): below 100 % an "on" step opens the gate only when the
 * Park-Miller value for its position (seed + 1) * 16807^(step + 8) mod 2^31 - 1, computed exactly
 * in doubles as the JSFX does, is under Probability. At 100 % the original code runs.
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
    double probPct = 100.0, seed = 0.0;
};

// (x * y) mod 2147483647, exact in doubles (y split into 16-bit halves), as the JSFX's pm_mul
inline double pmMul(double x, double y) noexcept
{
    const double hi = std::floor(y / 65536.0);
    double r = x * hi;
    r -= std::floor(r / 2147483647.0) * 2147483647.0;
    r = r * 65536.0 + x * (y - hi * 65536.0);
    r -= std::floor(r / 2147483647.0) * 2147483647.0;
    if (r < 0.0)
        r += 2147483647.0;
    if (r >= 2147483647.0)
        r -= 2147483647.0;
    return r;
}

// the Park-Miller value (0..1) for step i of seed sd, as the JSFX's pm_step
inline double pmStep(double i, double sd) noexcept
{
    double e = i - std::floor(i / 2147483646.0) * 2147483646.0 + 8.0;
    double b = 16807.0, p = sd + 1.0;
    while (e > 0.0)
    {
        if (e - std::floor(e / 2.0) * 2.0 > 0.0)
            p = pmMul(p, b);
        b = pmMul(b, b);
        e = std::floor(e / 2.0);
    }
    return p / 2147483647.0;
}

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
        fRndPos = 0.0;
        fRndKeep = 1.0;
        updateParams();
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    int currentStep() const noexcept { return fCur; }
    bool stepKept() const noexcept { return fProb >= 1.0 || fRndKeep != 0.0; } // Probability let this step play

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
            {
                fStepPos = beatPos * fStepsPerBeat;
                fRndPos = fStepPos;
            }
        }
        for (uint32_t i = 0; i < frames; ++i)
        {
            fStepPos += fStepInc;
            fRndPos += fStepInc;
            if (fStepPos >= 16.0)
                fStepPos -= 16.0;
            const double s = std::floor(fStepPos);
            const double cur = s - 16.0 * std::floor(s / 16.0);
            fCur = int(cur + 0.0001);
            const double on = fP.steps[fCur & 15] ? 1.0 : 0.0;
            double target;
            if (fProb < 1.0)
            {
                // Probability: a new draw whenever the step changes
                const double ri = std::floor(fRndPos);
                if (std::fabs(ri - fRndLast) >= 0.00001) // EEL2 != is fuzzy
                {
                    fRndLast = ri;
                    fRndKeep = pmStep(ri, fP.seed) < fProb ? 1.0 : 0.0;
                }
                target = 1.0 - fDepth * (1.0 - on * fRndKeep);
            }
            else
                target = 1.0 - fDepth * (1.0 - on);
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
        fProb = fP.probPct / 100.0;
        fRndLast = -0.5;
    }

    Params fP;
    double fSr = 48000.0, fStepPos = 0.0, fStepInc = 0.0, fGain = 1.0;
    double fStepsPerBeat = 4.0, fAttack = 0.0, fRelease = 0.0, fDepth = 1.0;
    double fRndPos = 0.0, fRndLast = -0.5, fRndKeep = 1.0, fProb = 1.0;
    int fCur = 0;
};

} // namespace nulltrance
