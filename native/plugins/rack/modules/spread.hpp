/*
 * NULL JSFX - Spread as a Rack module: drives nullspread::SpreadProcessor as plugins/spread/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../spread/SpreadDSP.hpp"

namespace nullrack {
namespace spread {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class SpreadModule final : public nulljsfx::Module {
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
    nullspread::SpreadProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new SpreadModule(); }

static const nulljsfx::ModuleDesc kDesc = { "spread", "Spread", "One Slider", kSliders, 1, create };

} // namespace spread
} // namespace nullrack
