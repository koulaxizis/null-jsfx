/*
 * NULL JSFX - Maximizer as a Rack module: drives nullmaximizer::MaximizerProcessor as plugins/maximizer/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../maximizer/MaximizerDSP.hpp"
#include "../../maximizer/MaximizerSliders.hpp"

#include <cmath>

namespace nullrack {
namespace maximizer {

class MaximizerModule final : public nulljsfx::Module {
public:
    MaximizerModule()
    {
        for (uint32_t i = 0; i < nullmaximizer::kNumSliders; ++i)
            fV[i] = nullmaximizer::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullmaximizer::kNumSliders)
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

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay = look

private:
    void update()
    {
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4], fV[5] >= 0.5f, int(std::lround(fV[6])), fV[7] >= 0.5f,
                       int(std::lround(fV[8])), fV[9] >= 0.5f, std::round(fV[10]), std::round(fV[11]));
    }

    nullmaximizer::MaximizerProcessor fDsp;
    float fV[nullmaximizer::kNumSliders];
};

inline nulljsfx::Module* create() { return new MaximizerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "maximizer", "Maximizer", "Essentials", nullmaximizer::kSliders,
                                            nullmaximizer::kNumSliders, create };

} // namespace maximizer
} // namespace nullrack
