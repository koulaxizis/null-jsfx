/*
 * NULL JSFX - Saturation as a Rack module: drives nullsaturation::SaturationProcessor as plugins/saturation/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../saturation/SaturationDSP.hpp"
#include "../../saturation/SaturationSliders.hpp"

#include <cmath>

namespace nullrack {
namespace saturation {

class SaturationModule final : public nulljsfx::Module {
public:
    SaturationModule()
    {
        for (uint32_t i = 0; i < nullsaturation::kNumSliders; ++i)
            fV[i] = nullsaturation::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullsaturation::kNumSliders)
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

    nullsaturation::SaturationProcessor fDsp;
    float fV[nullsaturation::kNumSliders];
};

inline nulljsfx::Module* create() { return new SaturationModule(); }

static const nulljsfx::ModuleDesc kDesc = { "saturation", "Saturation", "Essentials", nullsaturation::kSliders,
                                            nullsaturation::kNumSliders, create };

} // namespace saturation
} // namespace nullrack
