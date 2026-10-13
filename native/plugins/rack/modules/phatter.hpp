/*
 * NULL JSFX - Phatter as a Rack module: drives nullphatter::PhatterProcessor as plugins/phatter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../phatter/PhatterDSP.hpp"

namespace nullrack {
namespace phatter {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class PhatterModule final : public nulljsfx::Module {
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
    nullphatter::PhatterProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new PhatterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "phatter", "Phatter", "One Slider", kSliders, 1, create };

} // namespace phatter
} // namespace nullrack
