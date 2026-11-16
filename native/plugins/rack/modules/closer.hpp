/*
 * NULL JSFX - Closer as a Rack module: drives nullcloser::CloserProcessor as plugins/closer/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../closer/CloserDSP.hpp"

namespace nullrack {
namespace closer {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 40.0f, "" },
};

class CloserModule final : public nulljsfx::Module {
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
    nullcloser::CloserProcessor fDsp;
    float fAmount = 40.0f;
};

inline nulljsfx::Module* create() { return new CloserModule(); }

static const nulljsfx::ModuleDesc kDesc = { "closer", "Closer", "One Slider", kSliders, 1, create };

} // namespace closer
} // namespace nullrack
