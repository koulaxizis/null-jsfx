/*
 * NULL JSFX - OTT as a Rack module: drives nullott::OttProcessor as plugins/ott/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../ott/OttDSP.hpp"
#include "../../ott/OttSliders.hpp"

namespace nullrack {
namespace ott {

class OttModule final : public nulljsfx::Module {
public:
    OttModule()
    {
        for (uint32_t i = 0; i < nullott::kNumSliders; ++i)
            fV[i] = nullott::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullott::kNumSliders)
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
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4], fV[5], fV[6], fV[7]);
    }

    nullott::OttProcessor fDsp;
    float fV[nullott::kNumSliders];
};

inline nulljsfx::Module* create() { return new OttModule(); }

static const nulljsfx::ModuleDesc kDesc = { "ott", "OTT", "Advanced", nullott::kSliders,
                                            nullott::kNumSliders, create };

} // namespace ott
} // namespace nullrack
