/*
 * NULL JSFX - Harmonizer as a Rack module: drives nullharmonizer::HarmonizerProcessor as plugins/harmonizer/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../harmonizer/HarmonizerDSP.hpp"
#include "../../harmonizer/HarmonizerSliders.hpp"

#include <cmath>

namespace nullrack {
namespace harmonizer {

class HarmonizerModule final : public nulljsfx::Module {
public:
    HarmonizerModule()
    {
        for (uint32_t i = 0; i < nullharmonizer::kNumSliders; ++i)
            fV[i] = nullharmonizer::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // @init then @slider
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullharmonizer::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // the Rack has no MIDI: Mode MIDI holds no note and its voices stay silent
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
        double s[nullharmonizer::kNumSliders];
        for (uint32_t i = 0; i < nullharmonizer::kNumSliders; ++i)
            s[i] = sliderValue(nullharmonizer::kSliders[i], fV[i]);
        fDsp.setParams(s);
    }

    nullharmonizer::HarmonizerProcessor fDsp;
    float fV[nullharmonizer::kNumSliders];
};

inline nulljsfx::Module* create() { return new HarmonizerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "harmonizer", "Harmonizer", "Advanced", nullharmonizer::kSliders,
                                            nullharmonizer::kNumSliders, create };

} // namespace harmonizer
} // namespace nullrack
