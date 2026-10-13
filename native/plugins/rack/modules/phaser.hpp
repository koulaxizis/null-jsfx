/*
 * NULL JSFX - Phaser as a Rack module: drives nullphaser::PhaserProcessor as plugins/phaser/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../phaser/PhaserDSP.hpp"
#include "../../phaser/PhaserSliders.hpp"

namespace nullrack {
namespace phaser {

class PhaserModule final : public nulljsfx::Module {
public:
    PhaserModule()
    {
        for (uint32_t i = 0; i < nullphaser::kNumSliders; ++i)
            fV[i] = nullphaser::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullphaser::kNumSliders)
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
        using nullphaser::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 5.0, 0.1), fV[1], fV[2]);
    }

    nullphaser::PhaserProcessor fDsp;
    float fV[nullphaser::kNumSliders];
};

inline nulljsfx::Module* create() { return new PhaserModule(); }

static const nulljsfx::ModuleDesc kDesc = { "phaser", "Phaser", "Essentials", nullphaser::kSliders,
                                            nullphaser::kNumSliders, create };

} // namespace phaser
} // namespace nullrack
