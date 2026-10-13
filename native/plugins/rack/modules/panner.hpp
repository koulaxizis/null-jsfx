/*
 * NULL JSFX - Panner as a Rack module: drives nullpanner::PannerProcessor as plugins/panner/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../panner/PannerDSP.hpp"
#include "../../panner/PannerSliders.hpp"

#include <cmath>

namespace nullrack {
namespace panner {

class PannerModule final : public nulljsfx::Module {
public:
    PannerModule()
    {
        for (uint32_t i = 0; i < nullpanner::kNumSliders; ++i)
            fV[i] = nullpanner::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullpanner::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // Plugin.cpp's tap callback only feeds the vectorscope
        fDsp.process(inL, inR, outL, outR, frames, [](double, double) {});
    }

private:
    double v(uint32_t i) const { return nullpanner::sliderValue(i, fV[i]); }

    void update()
    {
        fDsp.setParams(v(0), int(std::lround(v(1))), v(2), v(3), v(4));
    }

    nullpanner::PannerProcessor fDsp;
    float fV[nullpanner::kNumSliders];
};

inline nulljsfx::Module* create() { return new PannerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "panner", "Panner", "Essentials", nullpanner::kSliders,
                                            nullpanner::kNumSliders, create };

} // namespace panner
} // namespace nullrack
