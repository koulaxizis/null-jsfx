/*
 * NULL JSFX - Breath Control as a Rack module: drives nullbreath::BreathProcessor as plugins/breath_control/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../breath_control/BreathControlDSP.hpp"
#include "../../breath_control/BreathControlSliders.hpp"

namespace nullrack {
namespace breath_control {

class BreathControlModule final : public nulljsfx::Module {
public:
    BreathControlModule()
    {
        for (uint32_t i = 0; i < nullbreath::kNumSliders; ++i)
            fV[i] = nullbreath::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullbreath::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    void update() { fDsp.setParams(fV[0], fV[1], fV[2], fV[3]); }

    nullbreath::BreathProcessor fDsp;
    float fV[nullbreath::kNumSliders];
};

inline nulljsfx::Module* create() { return new BreathControlModule(); }

static const nulljsfx::ModuleDesc kDesc = { "breath_control", "Breath Control", "Essentials", nullbreath::kSliders,
                                            nullbreath::kNumSliders, create };

} // namespace breath_control
} // namespace nullrack
