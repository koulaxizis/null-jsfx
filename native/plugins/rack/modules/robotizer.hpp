/*
 * NULL JSFX - Robotizer as a Rack module: drives nullrobotizer::RobotizerProcessor as plugins/robotizer/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../robotizer/RobotizerDSP.hpp"
#include "../../robotizer/RobotizerSliders.hpp"

namespace nullrack {
namespace robotizer {

class RobotizerModule final : public nulljsfx::Module {
public:
    RobotizerModule()
    {
        for (uint32_t i = 0; i < nullrobotizer::kNumSliders; ++i)
            fV[i] = nullrobotizer::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullrobotizer::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

    // as Plugin.cpp: pdc_delay = N
    uint32_t latency() const override { return uint32_t(nullrobotizer::RobotizerProcessor::latency()); }

private:
    void update()
    {
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3]);
    }

    nullrobotizer::RobotizerProcessor fDsp;
    float fV[nullrobotizer::kNumSliders];
};

inline nulljsfx::Module* create() { return new RobotizerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "robotizer", "Robotizer", "Advanced", nullrobotizer::kSliders,
                                            nullrobotizer::kNumSliders, create };

} // namespace robotizer
} // namespace nullrack
