/*
 * NULL JSFX - Exciter as a Rack module: drives nullexciter::ExciterProcessor as plugins/exciter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../exciter/ExciterDSP.hpp"

namespace nullrack {
namespace exciter {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 30.0f, "" },
};

class ExciterModule final : public nulljsfx::Module {
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
    nullexciter::ExciterProcessor fDsp;
    float fAmount = 30.0f;
};

inline nulljsfx::Module* create() { return new ExciterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "exciter", "Exciter", "One Slider", kSliders, 1, create };

} // namespace exciter
} // namespace nullrack
