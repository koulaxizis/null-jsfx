/*
 * NULL JSFX - Air as a Rack module: drives nullair::AirProcessor as plugins/air/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../air/AirDSP.hpp"

namespace nullrack {
namespace air {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 0.0f, "" },
};

class AirModule final : public nulljsfx::Module {
public:
    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
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
    nullair::AirProcessor fDsp;
    float fAmount = 0.0f;
};

inline nulljsfx::Module* create() { return new AirModule(); }

static const nulljsfx::ModuleDesc kDesc = { "air", "Air", "One Slider", kSliders, 1, create };

} // namespace air
} // namespace nullrack
