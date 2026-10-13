/*
 * NULL JSFX - GranularDelay as a Rack module: drives nullgranular::GranularDelayProcessor as plugins/granular_delay/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../granular_delay/GranularDelayDSP.hpp"
#include "../../granular_delay/GranularDelaySliders.hpp"

#include <cmath>

namespace nullrack {
namespace granular_delay {

class GranularDelayModule final : public nulljsfx::Module {
public:
    GranularDelayModule()
    {
        for (uint32_t i = 0; i < nullgranular::kNumSliders; ++i)
            fV[i] = nullgranular::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // @init then @slider
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullgranular::kNumSliders)
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
    // as Plugin.cpp: the parameter as the JSFX holds it: a fractional step (0.1, 0.5) is a double
    // in the JSFX, so the float parameter is snapped back onto min + k * step in double
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
        double s[nullgranular::kNumSliders];
        for (uint32_t i = 0; i < nullgranular::kNumSliders; ++i)
            s[i] = sliderValue(nullgranular::kSliders[i], fV[i]);
        fDsp.setParams(s);
    }

    nullgranular::GranularDelayProcessor fDsp;
    float fV[nullgranular::kNumSliders];
};

inline nulljsfx::Module* create() { return new GranularDelayModule(); }

static const nulljsfx::ModuleDesc kDesc = { "granular_delay", "Granular Delay", "Advanced", nullgranular::kSliders,
                                            nullgranular::kNumSliders, create };

} // namespace granular_delay
} // namespace nullrack
