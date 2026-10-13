/*
 * NULL JSFX - WarpedDelay as a Rack module: drives nullwarped::WarpedDelayProcessor as plugins/warped_delay/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../warped_delay/WarpedDelayDSP.hpp"
#include "../../warped_delay/WarpedDelaySliders.hpp"

#include <cmath>

namespace nullrack {
namespace warped_delay {

class WarpedDelayModule final : public nulljsfx::Module {
public:
    WarpedDelayModule()
    {
        for (uint32_t i = 0; i < nullwarped::kNumSliders; ++i)
            fV[i] = nullwarped::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // @init then @slider
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullwarped::kNumSliders)
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
        double s[nullwarped::kNumSliders];
        for (uint32_t i = 0; i < nullwarped::kNumSliders; ++i)
            s[i] = sliderValue(nullwarped::kSliders[i], fV[i]);
        fDsp.setParams(s);
    }

    nullwarped::WarpedDelayProcessor fDsp;
    float fV[nullwarped::kNumSliders];
};

inline nulljsfx::Module* create() { return new WarpedDelayModule(); }

static const nulljsfx::ModuleDesc kDesc = { "warped_delay", "Warped Delay", "Advanced", nullwarped::kSliders,
                                            nullwarped::kNumSliders, create };

} // namespace warped_delay
} // namespace nullrack
