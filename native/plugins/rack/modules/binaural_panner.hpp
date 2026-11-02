/*
 * NULL JSFX - BinauralPanner as a Rack module: drives nullbp::BinauralPanner as plugins/binaural_panner/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../binaural_panner/BinauralPannerDSP.hpp"
#include "../../binaural_panner/BinauralPannerSliders.hpp"

namespace nullrack {
namespace binaural_panner {

class BinauralPannerModule final : public nulljsfx::Module {
public:
    BinauralPannerModule()
    {
        for (uint32_t i = 0; i < nullbp::kNumSliders; ++i)
            fV[i] = nullbp::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        nullbp::apply(fDsp, fV);
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullbp::kNumSliders)
            return;
        fV[index] = v;
        nullbp::apply(fDsp, fV);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    nullbp::BinauralPanner fDsp;
    float fV[nullbp::kNumSliders];
};

inline nulljsfx::Module* create() { return new BinauralPannerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "binaural_panner", "Binaural Panner", "Advanced", nullbp::kSliders,
                                            nullbp::kNumSliders, create };

} // namespace binaural_panner
} // namespace nullrack
