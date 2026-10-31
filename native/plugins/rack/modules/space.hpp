/*
 * NULL JSFX - Space as a Rack module: drives nullspace::SpaceProcessor as plugins/space/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../space/SpaceDSP.hpp"
#include "../../space/SpaceSliders.hpp"

namespace nullrack {
namespace space {

class SpaceModule final : public nulljsfx::Module {
public:
    SpaceModule()
    {
        for (uint32_t i = 0; i < nullspace::kNumSliders; ++i)
            fV[i] = nullspace::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override { fDsp.setSampleRate(sr); }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullspace::kNumSliders)
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
        fDsp.setParams(nullspace::paramsFrom([this](uint32_t i) { return nullspace::sliderValue(i, fV[i]); }));
    }

    nullspace::SpaceProcessor fDsp;
    float fV[nullspace::kNumSliders];
};

inline nulljsfx::Module* create() { return new SpaceModule(); }

static const nulljsfx::ModuleDesc kDesc = { "space", "Space", "Essentials", nullspace::kSliders,
                                            nullspace::kNumSliders, create };

} // namespace space
} // namespace nullrack
