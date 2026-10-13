/*
 * NULL JSFX - RingMod as a Rack module: drives nullringmod::RingModProcessor as plugins/ring_mod/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../ring_mod/RingModDSP.hpp"
#include "../../ring_mod/RingModSliders.hpp"

namespace nullrack {
namespace ring_mod {

class RingModModule final : public nulljsfx::Module {
public:
    RingModModule()
    {
        for (uint32_t i = 0; i < nullringmod::kNumSliders; ++i)
            fV[i] = nullringmod::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullringmod::kNumSliders)
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
        using nullringmod::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 20.0, 0.1), onGrid(fV[1], 100.0, 2000.0, 10.0), fV[2], fV[3]);
    }

    nullringmod::RingModProcessor fDsp;
    float fV[nullringmod::kNumSliders];
};

inline nulljsfx::Module* create() { return new RingModModule(); }

static const nulljsfx::ModuleDesc kDesc = { "ring_mod", "Ring Mod", "Essentials", nullringmod::kSliders,
                                            nullringmod::kNumSliders, create };

} // namespace ring_mod
} // namespace nullrack
