/*
 * NULL JSFX - CombFilter as a Rack module: drives nullcomb::CombProcessor as plugins/comb_filter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../comb_filter/CombFilterDSP.hpp"
#include "../../comb_filter/CombFilterSliders.hpp"

#include <cmath>

namespace nullrack {
namespace comb_filter {

class CombFilterModule final : public nulljsfx::Module {
public:
    CombFilterModule()
    {
        for (uint32_t i = 0; i < nullcomb::kNumSliders; ++i)
            fV[i] = nullcomb::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullcomb::kNumSliders)
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
    double v(uint32_t i) const { return nullcomb::sliderValue(i, fV[i]); }
    void update() { fDsp.setParams(v(0), v(1), int(std::lround(v(2))), v(3)); }

    nullcomb::CombProcessor fDsp;
    float fV[nullcomb::kNumSliders];
};

inline nulljsfx::Module* create() { return new CombFilterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "comb_filter", "Comb Filter", "Advanced", nullcomb::kSliders,
                                            nullcomb::kNumSliders, create };

} // namespace comb_filter
} // namespace nullrack
