/*
 * NULL JSFX - Dynamics as a Rack module: drives nulldynamics::DynamicsProcessor as plugins/dynamics/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The Rack's modules are stereo, so this one has no sidechain input: Sidechain = External keys
 * the detector from the slot's own input (the JSFX's External with the track on pins 3/4). It
 * is the same as Internal, except with Channels Mid / Side, where Internal hears only that half.
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
            fV[i] = nulldynamics::kRackSliders[i].def;
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
            fDsp.processSample(l, r, inL[i], inR[i]); // the key is the slot's own input (see the top)
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    void update()
    {
        fDsp.setParams(int(std::lround(fV[0])), fV[1], fV[2], fV[3], fV[4], fV[5], fV[6], int(std::lround(fV[7])), fV[8],
                       fV[9], fV[10] > 0.5f, // External: keyed from the slot's own input
                       int(std::lround(fV[11])), fV[12] >= 0.5f);
    }

    nulldynamics::DynamicsProcessor fDsp;
    float fV[nulldynamics::kNumSliders];
};

inline nulljsfx::Module* create() { return new DynamicsModule(); }

// kRackSliders: Style without UPWARD, so saved Rack slots keep their meaning (see DynamicsSliders.hpp)
static const nulljsfx::ModuleDesc kDesc = { "dynamics", "Dynamics", "Essentials", nulldynamics::kRackSliders,
                                            nulldynamics::kNumSliders, create };

} // namespace dynamics
} // namespace nullrack
