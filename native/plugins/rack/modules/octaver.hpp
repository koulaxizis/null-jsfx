/*
 * NULL JSFX - Octaver as a Rack module: drives nulloctaver::OctaverProcessor as plugins/octaver/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../octaver/OctaverDSP.hpp"
#include "../../octaver/OctaverSliders.hpp"

namespace nullrack {
namespace octaver {

class OctaverModule final : public nulljsfx::Module {
public:
    OctaverModule()
    {
        for (uint32_t i = 0; i < nulloctaver::kNumSliders; ++i)
            fV[i] = nulloctaver::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulloctaver::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // Plugin.cpp's probe callback only feeds the display
        fDsp.process(inL, inR, outL, outR, frames, [](const nulloctaver::Probe&, double, double) {});
    }

private:
    void update()
    {
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4], fV[5]);
    }

    nulloctaver::OctaverProcessor fDsp;
    float fV[nulloctaver::kNumSliders];
};

inline nulljsfx::Module* create() { return new OctaverModule(); }

static const nulljsfx::ModuleDesc kDesc = { "octaver", "Octaver", "Essentials", nulloctaver::kSliders,
                                            nulloctaver::kNumSliders, create };

} // namespace octaver
} // namespace nullrack
