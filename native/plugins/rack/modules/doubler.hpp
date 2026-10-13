/*
 * NULL JSFX - Doubler as a Rack module: drives nulldoubler::DoublerProcessor as plugins/doubler/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../doubler/DoublerDSP.hpp"

namespace nullrack {
namespace doubler {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 40.0f, "" },
};

class DoublerModule final : public nulljsfx::Module {
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
    nulldoubler::DoublerProcessor fDsp;
    float fAmount = kSliders[0].def;
};

inline nulljsfx::Module* create() { return new DoublerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "doubler", "Doubler", "One Slider", kSliders, 1, create };

} // namespace doubler
} // namespace nullrack
