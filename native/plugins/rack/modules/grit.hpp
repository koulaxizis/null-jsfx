/*
 * NULL JSFX - Grit as a Rack module: drives nullgrit::GritProcessor as plugins/grit/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../grit/GritDSP.hpp"

namespace nullrack {
namespace grit {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Grit Intensity", "grit_intensity", 0.0f, 100.0f, 0.0f, "" },
};

class GritModule final : public nulljsfx::Module {
public:
    void setSampleRate(double sr) override
    {
        (void)sr; // as Plugin.cpp: the DSP has no rate
        fDsp.setAmount(fAmount);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index != 0)
            return;
        fAmount = v;
        fDsp.setAmount(v);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    nullgrit::GritProcessor fDsp;
    float fAmount = 0.0f;
};

inline nulljsfx::Module* create() { return new GritModule(); }

static const nulljsfx::ModuleDesc kDesc = { "grit", "Grit", "One Slider", kSliders, 1, create };

} // namespace grit
} // namespace nullrack
