/*
 * NULL JSFX - SubBloom as a Rack module: drives nullsubbloom::SubBloomProcessor as plugins/sub_bloom/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../sub_bloom/SubBloomDSP.hpp"
#include "../../sub_bloom/SubBloomSliders.hpp"

#include <cmath>

namespace nullrack {
namespace sub_bloom {

class SubBloomModule final : public nulljsfx::Module {
public:
    SubBloomModule()
    {
        for (uint32_t i = 0; i < nullsubbloom::kNumSliders; ++i)
            fV[i] = nullsubbloom::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullsubbloom::kNumSliders)
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
    double v(uint32_t i) const { return nullsubbloom::sliderValue(i, fV[i]); }

    void update()
    {
        fDsp.setParams(v(0), v(1), int(std::lround(v(2))), v(3), v(4));
    }

    nullsubbloom::SubBloomProcessor fDsp;
    float fV[nullsubbloom::kNumSliders];
};

inline nulljsfx::Module* create() { return new SubBloomModule(); }

static const nulljsfx::ModuleDesc kDesc = { "sub_bloom", "Sub Bloom", "Advanced", nullsubbloom::kSliders,
                                            nullsubbloom::kNumSliders, create };

} // namespace sub_bloom
} // namespace nullrack
