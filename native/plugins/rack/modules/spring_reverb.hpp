/*
 * NULL JSFX - SpringReverb as a Rack module: drives nullspring::SpringReverbProcessor as plugins/spring_reverb/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../spring_reverb/SpringReverbDSP.hpp"
#include "../../spring_reverb/SpringReverbSliders.hpp"

#include <cmath>

namespace nullrack {
namespace spring_reverb {

class SpringReverbModule final : public nulljsfx::Module {
public:
    SpringReverbModule()
    {
        for (uint32_t i = 0; i < nullspring::kNumSliders; ++i)
            fV[i] = nullspring::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // @init then @slider
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullspring::kNumSliders)
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
        double s[nullspring::kNumSliders];
        for (uint32_t i = 0; i < nullspring::kNumSliders; ++i)
            s[i] = sliderValue(nullspring::kSliders[i], fV[i]);
        fDsp.setParams(s);
    }

    nullspring::SpringReverbProcessor fDsp;
    float fV[nullspring::kNumSliders];
};

inline nulljsfx::Module* create() { return new SpringReverbModule(); }

static const nulljsfx::ModuleDesc kDesc = { "spring_reverb", "Spring Reverb", "Advanced", nullspring::kSliders,
                                            nullspring::kNumSliders, create };

} // namespace spring_reverb
} // namespace nullrack
