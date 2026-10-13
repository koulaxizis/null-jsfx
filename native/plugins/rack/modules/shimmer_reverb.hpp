/*
 * NULL JSFX - ShimmerReverb as a Rack module: drives nullshimmer::ShimmerReverbProcessor as plugins/shimmer_reverb/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../shimmer_reverb/ShimmerReverbDSP.hpp"
#include "../../shimmer_reverb/ShimmerReverbSliders.hpp"

#include <cmath>

namespace nullrack {
namespace shimmer_reverb {

class ShimmerReverbModule final : public nulljsfx::Module {
public:
    ShimmerReverbModule()
    {
        for (uint32_t i = 0; i < nullshimmer::kNumSliders; ++i)
            fV[i] = nullshimmer::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // @init then @slider
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullshimmer::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    // the parameter as the JSFX holds it: a fractional step (0.1, 0.5) is a double in the JSFX,
    // so the float parameter is snapped back onto min + k * step in double (as REAPER does)
    static double sliderValue(const DISTRHO::SliderInfo& in, float v)
    {
        if (in.step <= 0.0f || in.step >= 1.0f || in.options)
            return v;
        const double per = std::round(1.0 / double(in.step)); // steps per unit, e.g. 10
        const double st = 1.0 / per, lo = std::round(double(in.min) * per) / per;
        return std::fmin(double(in.max), lo + std::round((double(v) - lo) / st) * st);
    }

    void update()
    {
        double s[nullshimmer::kNumSliders];
        for (uint32_t i = 0; i < nullshimmer::kNumSliders; ++i)
            s[i] = sliderValue(nullshimmer::kSliders[i], fV[i]);
        fDsp.setParams(s);
    }

    nullshimmer::ShimmerReverbProcessor fDsp;
    float fV[nullshimmer::kNumSliders];
};

inline nulljsfx::Module* create() { return new ShimmerReverbModule(); }

static const nulljsfx::ModuleDesc kDesc = { "shimmer_reverb", "Shimmer Reverb", "Advanced", nullshimmer::kSliders,
                                            nullshimmer::kNumSliders, create };

} // namespace shimmer_reverb
} // namespace nullrack
