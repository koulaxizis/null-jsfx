/*
 * NULL JSFX - LadderFilter as a Rack module: drives nullladder::LadderProcessor as plugins/ladder_filter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../ladder_filter/LadderFilterDSP.hpp"
#include "../../ladder_filter/LadderFilterSliders.hpp"

#include <cmath>

namespace nullrack {
namespace ladder_filter {

class LadderFilterModule final : public nulljsfx::Module {
public:
    LadderFilterModule()
    {
        for (uint32_t i = 0; i < nullladder::kNumSliders; ++i)
            fV[i] = nullladder::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullladder::kNumSliders)
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
    double v(uint32_t i) const { return nullladder::sliderValue(i, fV[i]); }

    void update()
    {
        fDsp.setParams(v(0), v(1), v(2), int(std::lround(v(3))), v(4));
    }

    nullladder::LadderProcessor fDsp;
    float fV[nullladder::kNumSliders];
};

inline nulljsfx::Module* create() { return new LadderFilterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "ladder_filter", "Ladder Filter", "Advanced", nullladder::kSliders,
                                            nullladder::kNumSliders, create };

} // namespace ladder_filter
} // namespace nullrack
