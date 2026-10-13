/*
 * NULL JSFX - DJFilter as a Rack module: drives nulldjfilter::DjFilterProcessor as plugins/dj_filter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../dj_filter/DjFilterDSP.hpp"

namespace nullrack {
namespace dj_filter {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Filter", "filter", -100.0f, 100.0f, 0.0f, "", 1.0f },
};

class DjFilterModule final : public nulljsfx::Module {
public:
    // as Plugin.cpp's reinit(): @init, then @slider with the current value
    void setSampleRate(double sr) override
    {
        fDsp.init(sr);
        fDsp.setFilter(fFilter);
    }

    void setValue(uint32_t index, float v) override
    {
        if (index != 0)
            return;
        fFilter = v;
        fDsp.setFilter(v);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    nulldjfilter::DjFilterProcessor fDsp;
    float fFilter = kSliders[0].def;
};

inline nulljsfx::Module* create() { return new DjFilterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "dj_filter", "DJ Filter", "One Slider", kSliders, 1, create };

} // namespace dj_filter
} // namespace nullrack
