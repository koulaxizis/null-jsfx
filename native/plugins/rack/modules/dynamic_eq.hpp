/*
 * NULL JSFX - DynamicEQ as a Rack module: drives nulldeq::DynamicEqProcessor as plugins/dynamic_eq/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../dynamic_eq/DynamicEqDSP.hpp"
#include "../../dynamic_eq/DynamicEqSliders.hpp"

namespace nullrack {
namespace dynamic_eq {

class DynamicEqModule final : public nulljsfx::Module {
public:
    DynamicEqModule()
    {
        for (uint32_t i = 0; i < nulldeq::kNumSliders; ++i)
            fV[i] = nulldeq::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldeq::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    void update()
    {
        fDsp.setParams(nulldeq::params([this](uint32_t i) { return double(fV[i]); }));
    }

    nulldeq::DynamicEqProcessor fDsp;
    float fV[nulldeq::kNumSliders];
};

inline nulljsfx::Module* create() { return new DynamicEqModule(); }

static const nulljsfx::ModuleDesc kDesc = { "dynamic_eq", "Dynamic EQ", "Essentials", nulldeq::kSliders,
                                            nulldeq::kNumSliders, create };

} // namespace dynamic_eq
} // namespace nullrack
