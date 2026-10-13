/*
 * NULL JSFX - Formant Shift as a Rack module: drives nullformant::FormantShiftProcessor as plugins/formant_shift/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../formant_shift/FormantShiftDSP.hpp"

namespace nullrack {
namespace formant_shift {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Formant Position", "formant_position", 0.0f, 100.0f, 50.0f, "" },
};

class FormantShiftModule final : public nulljsfx::Module {
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
    nullformant::FormantShiftProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new FormantShiftModule(); }

static const nulljsfx::ModuleDesc kDesc = { "formant_shift", "Formant Shift", "One Slider", kSliders, 1, create };

} // namespace formant_shift
} // namespace nullrack
