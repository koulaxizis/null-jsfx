/*
 * NULL JSFX - Sweeper as a Rack module: drives nullsweeper::SweeperProcessor as plugins/sweeper/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../sweeper/SweeperDSP.hpp"

namespace nullrack {
namespace sweeper {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class SweeperModule final : public nulljsfx::Module {
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
    nullsweeper::SweeperProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new SweeperModule(); }

static const nulljsfx::ModuleDesc kDesc = { "sweeper", "Sweeper", "One Slider", kSliders, 1, create };

} // namespace sweeper
} // namespace nullrack
