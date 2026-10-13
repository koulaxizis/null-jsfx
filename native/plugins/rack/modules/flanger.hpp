/*
 * NULL JSFX - Flanger as a Rack module: drives nullflanger::FlangerProcessor as plugins/flanger/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../flanger/FlangerDSP.hpp"
#include "../../flanger/FlangerSliders.hpp"

namespace nullrack {
namespace flanger {

class FlangerModule final : public nulljsfx::Module {
public:
    FlangerModule()
    {
        for (uint32_t i = 0; i < nullflanger::kNumSliders; ++i)
            fV[i] = nullflanger::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullflanger::kNumSliders)
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
        using nullflanger::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 5.0, 0.1), onGrid(fV[1], 1.0, 20.0, 0.1), fV[2], fV[3]);
    }

    nullflanger::FlangerProcessor fDsp;
    float fV[nullflanger::kNumSliders];
};

inline nulljsfx::Module* create() { return new FlangerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "flanger", "Flanger", "Essentials", nullflanger::kSliders,
                                            nullflanger::kNumSliders, create };

} // namespace flanger
} // namespace nullrack
