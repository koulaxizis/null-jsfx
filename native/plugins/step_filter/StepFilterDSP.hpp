/*
 * NULL JSFX - Step Filter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/step_filter.jsfx, computed in double like EEL2: a state-variable
 * filter (TPT) whose cutoff follows up to 16 steps. One step = Step Length at the host tempo,
 * locked to the host's beat position while the transport plays (re-read every block, as the
 * JSFX's @block does), free-running at the tempo when stopped. A step's value v sets the cutoff
 * Low * (High / Low)^v; Glide moves from the previous value over the first Glide share of the
 * step (a raised cosine). Seed 0 plays the Step sliders, any other Seed 16 values drawn from it
 * (stepValues, exact Park-Miller in doubles).
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullstepfilter {

struct Params {
    double steps[16] = {};  // Step 1..16, %
    int numSteps = 16;
    int stepLength = 2;     // 0..3: 1/4, 1/8, 1/16, 1/32
    double lowHz = 200.0, highHz = 5000.0;
    double resPct = 30.0;
    int type = 0;           // 0..2: Low Pass, Band Pass, High Pass
    double glidePct = 20.0;
    double seed = 0.0;
    double mixPct = 100.0;
};

// one Park-Miller step (x * 16807 mod 2^31 - 1, exact in doubles), as the JSFX's sf_pm
inline double sfPm(double x) noexcept
{
    x *= 16807.0;
    return x - std::floor(x / 2147483647.0) * 2147483647.0;
}

// the 16 values playing (0..1), as the JSFX's @slider: the Step sliders, or the Seed's values
inline void stepValues(const Params& p, double* sv) noexcept
{
    if (p.seed >= 0.5)
    {
        double x = p.seed * 13.0 + 4242.0;
        for (int i = 0; i < 4; ++i)
            x = sfPm(x);
        for (int i = 0; i < 16; ++i)
        {
            x = sfPm(x);
            sv[i] = std::floor(x / 2147483647.0 * 11.0) / 10.0;
        }
    }
    else
        for (int i = 0; i < 16; ++i)
            sv[i] = p.steps[i] / 100.0;
}

inline int clampSteps(int n) noexcept { return n < 1 ? 1 : n > 16 ? 16 : n; }

class StepFilterProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fStepPos = 0.0;
        fCur = 0;
        fIc1L = fIc2L = fIc1R = fIc2R = 0.0;
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
        fStepInc = bpm / 60.0 * fStepsPerBeat / fSr;
        if (playing)
            fStepPos = beatPos * fStepsPerBeat;
        const double n = double(fN);
        for (uint32_t i = 0; i < frames; ++i)
        {
            fStepPos += fStepInc;
            if (fStepPos >= 16.0 * n)
                fStepPos -= 16.0 * n;
            const double s = std::floor(fStepPos);
            const double cur = s - n * std::floor(s / n);
            fCur = int(cur + 0.0001);
            double v = fSv[fCur & 15];
            if (fGlide > 0.0)
            {
                const double t = (fStepPos - s) / fGlide;
                if (t < 1.0)
                {
                    const double vp = fSv[fCur > 0 ? fCur - 1 : fN - 1];
                    v = vp + (v - vp) * (0.5 - 0.5 * std::cos(3.141592653589793 * t));
                }
            }
            const double fc = std::fmin(fP.lowHz * std::pow(fP.highHz / fP.lowHz, v), 0.45 * fSr);
            const double g = std::tan(3.141592653589793 * fc / fSr);
            const double a1 = 1.0 / (1.0 + g * (g + fK));
            const double a2 = g * a1;
            const double a3 = g * a2;
            outL[i] = static_cast<T>(tick(double(inL[i]), fIc1L, fIc2L, a1, a2, a3));
            outR[i] = static_cast<T>(tick(double(inR[i]), fIc1R, fIc2R, a1, a2, a3));
        }
    }

private:
    double tick(double x, double& ic1, double& ic2, double a1, double a2, double a3) const noexcept
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        const double y = fP.type == 0 ? v2 : fP.type == 1 ? fK * v1 : x - fK * v1 - v2;
        return x + (y - x) * fMix;
    }

    void updateParams()
    {
        fN = clampSteps(fP.numSteps);
        fStepsPerBeat = std::pow(2.0, double(fP.stepLength));
        fK = 1.0 / (0.5 * std::pow(24.0, fP.resPct / 100.0));
        fGlide = fP.glidePct / 100.0;
        fMix = fP.mixPct / 100.0;
        stepValues(fP, fSv);
    }

    Params fP;
    double fSr = 48000.0, fStepPos = 0.0, fStepInc = 0.0, fStepsPerBeat = 4.0;
    double fK = 1.0, fGlide = 0.2, fMix = 1.0;
    double fIc1L = 0.0, fIc2L = 0.0, fIc1R = 0.0, fIc2R = 0.0;
    double fSv[16] = {};
    int fN = 16, fCur = 0;
};

} // namespace nullstepfilter
