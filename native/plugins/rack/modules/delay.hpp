/*
 * NULL JSFX - Delay as a Rack module: drives nulldelay::DelayProcessor as plugins/delay/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../delay/DelayDSP.hpp"
#include "../../delay/DelaySliders.hpp"

#include <cmath>

namespace nullrack {
namespace delay {

class DelayModule final : public nulljsfx::Module {
public:
    DelayModule()
    {
        for (uint32_t i = 0; i < nulldelay::kNumSliders; ++i)
            fV[i] = nulldelay::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldelay::kNumSliders)
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
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3] >= 0.5f, int(std::lround(fV[4])), fV[5] >= 0.5f,
                       fV[6], fV[7], fV[8], fV[9], fV[10], fV[11], fV[12] >= 0.5f, fV[13] >= 0.5f,
                       std::round(fV[14]));
    }

    nulldelay::DelayProcessor fDsp;
    float fV[nulldelay::kNumSliders];
};

inline nulljsfx::Module* create() { return new DelayModule(); }

static const nulljsfx::ModuleDesc kDesc = { "delay", "Delay", "Essentials", nulldelay::kSliders,
                                            nulldelay::kNumSliders, create };

} // namespace delay
} // namespace nullrack
