/*
 * NULL JSFX - Riser as a Rack module: drives nullriser::RiserProcessor as plugins/riser/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../riser/RiserDSP.hpp"

namespace nullrack {
namespace riser {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Intensity", "intensity", 0.0f, 100.0f, 0.0f, "%" },
};

class RiserModule final : public nulljsfx::Module {
public:
    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.setAmount(fAmount);
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
    nullriser::RiserProcessor fDsp;
    float fAmount = 0.0f;
};

inline nulljsfx::Module* create() { return new RiserModule(); }

static const nulljsfx::ModuleDesc kDesc = { "riser", "Riser", "One Slider", kSliders, 1, create };

} // namespace riser
} // namespace nullrack
