/*
 * NULL JSFX - Stretch as a Rack module: drives nullstretch::StretchProcessor as plugins/stretch/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../stretch/StretchDSP.hpp"

namespace nullrack {
namespace stretch {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", -100.0f, 100.0f, 0.0f, "" },
};

class StretchModule final : public nulljsfx::Module {
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
    nullstretch::StretchProcessor fDsp;
    float fAmount = 0.0f;
};

inline nulljsfx::Module* create() { return new StretchModule(); }

static const nulljsfx::ModuleDesc kDesc = { "stretch", "Stretch", "One Slider", kSliders, 1, create };

} // namespace stretch
} // namespace nullrack
