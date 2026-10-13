/*
 * NULL JSFX - AmpSimulator as a Rack module: drives nullamp::AmpSimulatorProcessor as plugins/amp_simulator/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../amp_simulator/AmpSimulatorDSP.hpp"
#include "../../amp_simulator/AmpSimulatorSliders.hpp"

#include <cmath>

namespace nullrack {
namespace amp_simulator {

class AmpSimulatorModule final : public nulljsfx::Module {
public:
    AmpSimulatorModule()
    {
        for (uint32_t i = 0; i < nullamp::kNumSliders; ++i)
            fV[i] = nullamp::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullamp::kNumSliders)
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
        using nullamp::snap;
        fDsp.setParams(int(std::lround(fV[0])), snap(fV[1], 0.0), snap(fV[2], 0.0), snap(fV[3], 0.0),
                       snap(fV[4], 0.0), snap(fV[5], 0.0), snap(fV[6], 0.0), fV[7] >= 0.5f, fV[8],
                       fV[9] >= 0.5f, snap(fV[10], -24.0));
    }

    nullamp::AmpSimulatorProcessor fDsp;
    float fV[nullamp::kNumSliders];
};

inline nulljsfx::Module* create() { return new AmpSimulatorModule(); }

static const nulljsfx::ModuleDesc kDesc = { "amp_simulator", "Amp Simulator", "Advanced", nullamp::kSliders,
                                            nullamp::kNumSliders, create };

} // namespace amp_simulator
} // namespace nullrack
