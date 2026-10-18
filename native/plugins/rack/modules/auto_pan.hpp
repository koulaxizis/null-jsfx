/*
 * NULL JSFX - AutoPan as a Rack module: drives nullautopan::AutoPanProcessor as plugins/auto_pan/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../auto_pan/AutoPanDSP.hpp"
#include "../../auto_pan/AutoPanSliders.hpp"

#include <cmath>

namespace nullrack {
namespace auto_pan {

class AutoPanModule final : public nulljsfx::Module {
public:
    AutoPanModule()
    {
        for (uint32_t i = 0; i < nullautopan::kNumSliders; ++i)
            fV[i] = nullautopan::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullautopan::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when it reports one, else 120
        const double tempo = time.valid && time.tempo > 0.0 ? time.tempo : 120.0;
        fDsp.process(inL, inR, outL, outR, frames, tempo);
    }

private:
    void update()
    {
        using nullautopan::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 10.0, 0.1), fV[1], fV[2], fV[3] >= 0.5f, int(std::lround(fV[4])),
                       int(std::lround(fV[5])), std::round(fV[6]));
    }

    nullautopan::AutoPanProcessor fDsp;
    float fV[nullautopan::kNumSliders];
};

inline nulljsfx::Module* create() { return new AutoPanModule(); }

static const nulljsfx::ModuleDesc kDesc = { "auto_pan", "Auto Pan", "Essentials", nullautopan::kSliders,
                                            nullautopan::kNumSliders, create };

} // namespace auto_pan
} // namespace nullrack
