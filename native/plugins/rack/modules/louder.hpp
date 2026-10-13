/*
 * NULL JSFX - Louder as a Rack module: drives nulllouder::LouderProcessor as plugins/louder/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../louder/LouderDSP.hpp"

namespace nullrack {
namespace louder {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class LouderModule final : public nulljsfx::Module {
public:
    void setSampleRate(double sr) override
    {
        // @init (the JSFX's @slider follows; the gain does not depend on the rate)
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

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay = la

private:
    nulllouder::LouderProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new LouderModule(); }

static const nulljsfx::ModuleDesc kDesc = { "louder", "Louder", "One Slider", kSliders, 1, create };

} // namespace louder
} // namespace nullrack
