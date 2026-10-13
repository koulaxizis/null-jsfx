/*
 * NULL JSFX - Driver as a Rack module: drives nulldriver::DriverProcessor as plugins/driver/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../driver/DriverDSP.hpp"

namespace nullrack {
namespace driver {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class DriverModule final : public nulljsfx::Module {
public:
    void setSampleRate(double) override
    {
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
    nulldriver::DriverProcessor fDsp;
    float fAmount = kSliders[0].def;
};

inline nulljsfx::Module* create() { return new DriverModule(); }

static const nulljsfx::ModuleDesc kDesc = { "driver", "Driver", "One Slider", kSliders, 1, create };

} // namespace driver
} // namespace nullrack
