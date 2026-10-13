/*
 * NULL JSFX - ParametricEQ as a Rack module: drives nullpeq::ParametricEqProcessor as plugins/parametric_eq/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../parametric_eq/ParametricEqDSP.hpp"
#include "../../parametric_eq/ParametricEqSliders.hpp"

namespace nullrack {
namespace parametric_eq {

class ParametricEqModule final : public nulljsfx::Module {
public:
    ParametricEqModule()
    {
        for (uint32_t i = 0; i < nullpeq::kNumSliders; ++i)
            fV[i] = nullpeq::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullpeq::kNumSliders)
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
        nullpeq::BandParams p[nullpeq::kBands];
        nullpeq::bandParams([this](uint32_t i) { return double(fV[i]); }, p);
        fDsp.setParams(p);
    }

    nullpeq::ParametricEqProcessor fDsp;
    float fV[nullpeq::kNumSliders];
};

inline nulljsfx::Module* create() { return new ParametricEqModule(); }

static const nulljsfx::ModuleDesc kDesc = { "parametric_eq", "Parametric EQ", "Essentials", nullpeq::kSliders,
                                            nullpeq::kNumSliders, create };

} // namespace parametric_eq
} // namespace nullrack
