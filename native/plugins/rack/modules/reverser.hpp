/*
 * NULL JSFX - Reverser as a Rack module: drives nullreverser::ReverserProcessor as plugins/reverser/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../reverser/ReverserDSP.hpp"
#include "../../reverser/ReverserSliders.hpp"

#include <cmath>

namespace nullrack {
namespace reverser {

class ReverserModule final : public nulljsfx::Module {
public:
    ReverserModule()
    {
        for (uint32_t i = 0; i < nullreverser::kNumSliders; ++i)
            fV[i] = nullreverser::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullreverser::kNumSliders)
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
    double v(uint32_t i) const { return nullreverser::sliderValue(i, fV[i]); }

    void update()
    {
        fDsp.setParams(v(0), int(std::lround(v(1))), v(2), v(3));
    }

    nullreverser::ReverserProcessor fDsp;
    float fV[nullreverser::kNumSliders];
};

inline nulljsfx::Module* create() { return new ReverserModule(); }

static const nulljsfx::ModuleDesc kDesc = { "reverser", "Reverser", "Advanced", nullreverser::kSliders,
                                            nullreverser::kNumSliders, create };

} // namespace reverser
} // namespace nullrack
