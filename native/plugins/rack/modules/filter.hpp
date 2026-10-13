/*
 * NULL JSFX - Filter as a Rack module: drives nullfilter::FilterProcessor as plugins/filter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../filter/FilterDSP.hpp"
#include "../../filter/FilterSliders.hpp"

#include <cmath>

namespace nullrack {
namespace filter {

class FilterModule final : public nulljsfx::Module {
public:
    FilterModule()
    {
        for (uint32_t i = 0; i < nullfilter::kNumSliders; ++i)
            fV[i] = nullfilter::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullfilter::kNumSliders)
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
        fDsp.setParams(fV[0], fV[1], fV[2], int(std::lround(fV[3])));
    }

    nullfilter::FilterProcessor fDsp;
    float fV[nullfilter::kNumSliders];
};

inline nulljsfx::Module* create() { return new FilterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "filter", "Filter", "Essentials", nullfilter::kSliders,
                                            nullfilter::kNumSliders, create };

} // namespace filter
} // namespace nullrack
