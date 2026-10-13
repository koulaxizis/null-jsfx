/*
 * NULL JSFX - Vibrato as a Rack module: drives nullvibrato::VibratoProcessor as plugins/vibrato/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../vibrato/VibratoDSP.hpp"
#include "../../vibrato/VibratoSliders.hpp"

namespace nullrack {
namespace vibrato {

class VibratoModule final : public nulljsfx::Module {
public:
    VibratoModule()
    {
        for (uint32_t i = 0; i < nullvibrato::kNumSliders; ++i)
            fV[i] = nullvibrato::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullvibrato::kNumSliders)
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
        using nullvibrato::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 15.0, 0.1), onGrid(fV[1], 0.0, 20.0, 0.1), fV[2]);
    }

    nullvibrato::VibratoProcessor fDsp;
    float fV[nullvibrato::kNumSliders];
};

inline nulljsfx::Module* create() { return new VibratoModule(); }

static const nulljsfx::ModuleDesc kDesc = { "vibrato", "Vibrato", "Essentials", nullvibrato::kSliders,
                                            nullvibrato::kNumSliders, create };

} // namespace vibrato
} // namespace nullrack
