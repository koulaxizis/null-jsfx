/*
 * NULL JSFX - VinylAge as a Rack module: drives nullvinyl::VinylAgeProcessor as plugins/vinyl_age/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../vinyl_age/VinylAgeDSP.hpp"
#include "../../vinyl_age/VinylAgeSliders.hpp"

#include <cmath>

namespace nullrack {
namespace vinyl_age {

class VinylAgeModule final : public nulljsfx::Module {
public:
    VinylAgeModule()
    {
        for (uint32_t i = 0; i < nullvinyl::kNumSliders; ++i)
            fV[i] = nullvinyl::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // @init then @slider
        fDsp.setSampleRate(sr);
        fLatency = uint32_t(nullvinyl::VinylAgeProcessor::latencyFor(sr));
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullvinyl::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

    uint32_t latency() const override { return fLatency; }

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
        double s[nullvinyl::kNumSliders];
        for (uint32_t i = 0; i < nullvinyl::kNumSliders; ++i)
            s[i] = sliderValue(nullvinyl::kSliders[i], fV[i]);
        fDsp.setParams(s);
    }

    nullvinyl::VinylAgeProcessor fDsp;
    float fV[nullvinyl::kNumSliders];
    uint32_t fLatency = 0;
};

inline nulljsfx::Module* create() { return new VinylAgeModule(); }

static const nulljsfx::ModuleDesc kDesc = { "vinyl_age", "Vinyl Age", "Advanced", nullvinyl::kSliders,
                                            nullvinyl::kNumSliders, create };

} // namespace vinyl_age
} // namespace nullrack
