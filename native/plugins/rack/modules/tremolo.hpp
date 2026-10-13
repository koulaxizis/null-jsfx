/*
 * NULL JSFX - Tremolo as a Rack module: drives nulltremolo::TremoloProcessor as plugins/tremolo/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../tremolo/TremoloDSP.hpp"
#include "../../tremolo/TremoloSliders.hpp"

namespace nullrack {
namespace tremolo {

class TremoloModule final : public nulljsfx::Module {
public:
    TremoloModule()
    {
        for (uint32_t i = 0; i < nulltremolo::kNumSliders; ++i)
            fV[i] = nulltremolo::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltremolo::kNumSliders)
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
        using nulltremolo::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 20.0, 0.1), fV[1], fV[2]);
    }

    nulltremolo::TremoloProcessor fDsp;
    float fV[nulltremolo::kNumSliders];
};

inline nulljsfx::Module* create() { return new TremoloModule(); }

static const nulljsfx::ModuleDesc kDesc = { "tremolo", "Tremolo", "Essentials", nulltremolo::kSliders,
                                            nulltremolo::kNumSliders, create };

} // namespace tremolo
} // namespace nullrack
