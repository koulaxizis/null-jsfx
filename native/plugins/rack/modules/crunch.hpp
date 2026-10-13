/*
 * NULL JSFX - Crunch as a Rack module: drives nullcrunch::CrunchProcessor as plugins/crunch/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../crunch/CrunchDSP.hpp"

namespace nullrack {
namespace crunch {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class CrunchModule final : public nulljsfx::Module {
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
    nullcrunch::CrunchProcessor fDsp;
    float fAmount = kSliders[0].def;
};

inline nulljsfx::Module* create() { return new CrunchModule(); }

static const nulljsfx::ModuleDesc kDesc = { "crunch", "Crunch", "One Slider", kSliders, 1, create };

} // namespace crunch
} // namespace nullrack
