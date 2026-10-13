/*
 * NULL JSFX - StereoWidth as a Rack module: drives nullstereowidth::StereoWidthProcessor as plugins/stereo_width/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../stereo_width/StereoWidthDSP.hpp"
#include "../../stereo_width/StereoWidthSliders.hpp"

namespace nullrack {
namespace stereo_width {

class StereoWidthModule final : public nulljsfx::Module {
public:
    StereoWidthModule()
    {
        for (uint32_t i = 0; i < nullstereowidth::kNumSliders; ++i)
            fV[i] = nullstereowidth::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fSr = sr;
        fDsp.reset();
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullstereowidth::kNumSliders)
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
        fDsp.setParams(nullstereowidth::paramsFor(fV[0], fV[1], fV[2], fSr));
    }

    nullstereowidth::StereoWidthProcessor fDsp;
    float fV[nullstereowidth::kNumSliders];
    double fSr = 44100.0;
};

inline nulljsfx::Module* create() { return new StereoWidthModule(); }

static const nulljsfx::ModuleDesc kDesc = { "stereo_width", "Stereo Width", "Essentials", nullstereowidth::kSliders,
                                            nullstereowidth::kNumSliders, create };

} // namespace stereo_width
} // namespace nullrack
