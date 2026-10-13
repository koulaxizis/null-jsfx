/*
 * NULL JSFX - Tilt as a Rack module: drives nulltilt::TiltProcessor as plugins/tilt/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../tilt/TiltDSP.hpp"

namespace nullrack {
namespace tilt {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", -100.0f, 100.0f, 0.0f, "" },
};

class TiltModule final : public nulljsfx::Module {
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
    nulltilt::TiltProcessor fDsp;
    float fAmount = 0.0f;
};

inline nulljsfx::Module* create() { return new TiltModule(); }

static const nulljsfx::ModuleDesc kDesc = { "tilt", "Tilt", "One Slider", kSliders, 1, create };

} // namespace tilt
} // namespace nullrack
