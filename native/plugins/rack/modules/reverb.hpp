/*
 * NULL JSFX - Reverb as a Rack module: drives nullreverb::ReverbProcessor as plugins/reverb/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../reverb/ReverbDSP.hpp"
#include "../../reverb/ReverbSliders.hpp"

#include <cmath>

namespace nullrack {
namespace reverb {

class ReverbModule final : public nulljsfx::Module {
public:
    ReverbModule()
    {
        for (uint32_t i = 0; i < nullreverb::kNumSliders; ++i)
            fV[i] = nullreverb::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullreverb::kNumSliders)
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
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4] > 0.0f, fV[5], fV[6], fV[7], fV[8], fV[9], fV[10] > 0.0f,
                       int(std::lround(fV[11])), fV[12], fV[13], fV[14]);
    }

    nullreverb::ReverbProcessor fDsp;
    float fV[nullreverb::kNumSliders];
};

inline nulljsfx::Module* create() { return new ReverbModule(); }

static const nulljsfx::ModuleDesc kDesc = { "reverb", "Reverb", "Essentials", nullreverb::kSliders,
                                            nullreverb::kNumSliders, create };

} // namespace reverb
} // namespace nullrack
