/*
 * NULL JSFX - CabinetSimulator as a Rack module: drives nullcab::CabinetSimulatorProcessor as plugins/cabinet_simulator/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../cabinet_simulator/CabinetSimulatorDSP.hpp"
#include "../../cabinet_simulator/CabinetSimulatorSliders.hpp"

#include <cmath>

namespace nullrack {
namespace cabinet_simulator {

class CabinetSimulatorModule final : public nulljsfx::Module {
public:
    CabinetSimulatorModule()
    {
        for (uint32_t i = 0; i < nullcab::kNumSliders; ++i)
            fV[i] = nullcab::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullcab::kNumSliders)
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
    void update() { fDsp.setParams(int(std::lround(fV[0])), fV[1], fV[2], fV[3], fV[4], fV[5], fV[6]); }

    nullcab::CabinetSimulatorProcessor fDsp;
    float fV[nullcab::kNumSliders];
};

inline nulljsfx::Module* create() { return new CabinetSimulatorModule(); }

static const nulljsfx::ModuleDesc kDesc = { "cabinet_simulator", "Cabinet Simulator", "Essentials", nullcab::kSliders,
                                            nullcab::kNumSliders, create };

} // namespace cabinet_simulator
} // namespace nullrack
