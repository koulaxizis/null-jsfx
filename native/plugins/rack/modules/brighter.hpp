/*
 * NULL JSFX - Brighter as a Rack module: drives nullbrighter::BrighterProcessor as plugins/brighter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../brighter/BrighterDSP.hpp"

namespace nullrack {
namespace brighter {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class BrighterModule final : public nulljsfx::Module {
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
    nullbrighter::BrighterProcessor fDsp;
    float fAmount = kSliders[0].def;
};

inline nulljsfx::Module* create() { return new BrighterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "brighter", "Brighter", "One Slider", kSliders, 1, create };

} // namespace brighter
} // namespace nullrack
