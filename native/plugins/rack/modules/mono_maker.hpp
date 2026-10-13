/*
 * NULL JSFX - MonoMaker as a Rack module: drives nullmonomaker::MonoMakerProcessor as plugins/mono_maker/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../mono_maker/MonoMakerDSP.hpp"

namespace nullrack {
namespace mono_maker {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class MonoMakerModule final : public nulljsfx::Module {
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
    nullmonomaker::MonoMakerProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new MonoMakerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "mono_maker", "Mono Maker", "One Slider", kSliders, 1, create };

} // namespace mono_maker
} // namespace nullrack
