/*
 * NULL JSFX - Chorus as a Rack module: drives nullchorus::ChorusProcessor as plugins/chorus/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../chorus/ChorusDSP.hpp"
#include "../../chorus/ChorusSliders.hpp"

namespace nullrack {
namespace chorus {

class ChorusModule final : public nulljsfx::Module {
public:
    ChorusModule()
    {
        for (uint32_t i = 0; i < nullchorus::kNumSliders; ++i)
            fV[i] = nullchorus::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullchorus::kNumSliders)
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
        using nullchorus::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 20.0, 0.1), onGrid(fV[1], 0.0, 10.0, 0.1), fV[2]);
    }

    nullchorus::ChorusProcessor fDsp;
    float fV[nullchorus::kNumSliders];
};

inline nulljsfx::Module* create() { return new ChorusModule(); }

static const nulljsfx::ModuleDesc kDesc = { "chorus", "Chorus", "Essentials", nullchorus::kSliders,
                                            nullchorus::kNumSliders, create };

} // namespace chorus
} // namespace nullrack
