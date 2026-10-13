/*
 * NULL JSFX - Dynamics as a Rack module: drives nulldynamics::DynamicsProcessor as plugins/dynamics/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../dynamics/DynamicsDSP.hpp"
#include "../../dynamics/DynamicsSliders.hpp"

#include <cmath>

namespace nullrack {
namespace dynamics {

class DynamicsModule final : public nulljsfx::Module {
public:
    DynamicsModule()
    {
        for (uint32_t i = 0; i < nulldynamics::kNumSliders; ++i)
            fV[i] = nulldynamics::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldynamics::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    void update()
    {
        fDsp.setParams(int(std::lround(fV[0])), fV[1], fV[2], fV[3], fV[4], fV[5], fV[6]);
    }

    nulldynamics::DynamicsProcessor fDsp;
    float fV[nulldynamics::kNumSliders];
};

inline nulljsfx::Module* create() { return new DynamicsModule(); }

static const nulljsfx::ModuleDesc kDesc = { "dynamics", "Dynamics", "Essentials", nulldynamics::kSliders,
                                            nulldynamics::kNumSliders, create };

} // namespace dynamics
} // namespace nullrack
