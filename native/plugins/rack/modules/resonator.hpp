/*
 * NULL JSFX - Resonator as a Rack module: drives nullres::ResonatorProcessor as plugins/resonator/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../resonator/ResonatorDSP.hpp"
#include "../../resonator/ResonatorSliders.hpp"

namespace nullrack {
namespace resonator {

class ResonatorModule final : public nulljsfx::Module {
public:
    ResonatorModule()
    {
        for (uint32_t i = 0; i < nullres::kNumSliders; ++i)
            fV[i] = nullres::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullres::kNumSliders)
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
        double s[nullres::kNumSliders];
        for (uint32_t i = 0; i < nullres::kNumSliders; ++i)
            s[i] = nullres::sliderValue(i, fV[i]);
        fDsp.setParams(s);
    }

    nullres::ResonatorProcessor fDsp;
    float fV[nullres::kNumSliders];
};

inline nulljsfx::Module* create() { return new ResonatorModule(); }

static const nulljsfx::ModuleDesc kDesc = { "resonator", "Resonator", "Advanced", nullres::kSliders,
                                            nullres::kNumSliders, create };

} // namespace resonator
} // namespace nullrack
