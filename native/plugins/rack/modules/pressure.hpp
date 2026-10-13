/*
 * NULL JSFX - Pressure as a Rack module: drives nullpressure::PressureProcessor as plugins/pressure/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../pressure/PressureDSP.hpp"

namespace nullrack {
namespace pressure {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class PressureModule final : public nulljsfx::Module {
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
        // as Plugin.cpp's run(), without its display meters
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    nullpressure::PressureProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new PressureModule(); }

static const nulljsfx::ModuleDesc kDesc = { "pressure", "Pressure", "One Slider", kSliders, 1, create };

} // namespace pressure
} // namespace nullrack
