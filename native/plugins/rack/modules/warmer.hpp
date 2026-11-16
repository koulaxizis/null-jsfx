/*
 * NULL JSFX - Warmer as a Rack module: drives nullwarmer::WarmerProcessor as plugins/warmer/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../warmer/WarmerDSP.hpp"

namespace nullrack {
namespace warmer {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 40.0f, "" },
};

class WarmerModule final : public nulljsfx::Module {
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
    nullwarmer::WarmerProcessor fDsp;
    float fAmount = 40.0f;
};

inline nulljsfx::Module* create() { return new WarmerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "warmer", "Warmer", "One Slider", kSliders, 1, create };

} // namespace warmer
} // namespace nullrack
