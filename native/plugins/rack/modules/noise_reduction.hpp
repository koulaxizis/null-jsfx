/*
 * NULL JSFX - NoiseReduction as a Rack module: drives nullnr::NoiseReducer as plugins/noise_reduction/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../noise_reduction/NoiseReductionDSP.hpp"
#include "../../noise_reduction/NoiseReductionSliders.hpp"

#include <cmath>

namespace nullrack {
namespace noise_reduction {

class NoiseReductionModule final : public nulljsfx::Module {
public:
    NoiseReductionModule()
    {
        for (uint32_t i = 0; i < nullnr::kNumSliders; ++i)
            fV[i] = nullnr::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // like the JSFX's @init: the learnt profile survives unless the sample rate changes
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullnr::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

    // as Plugin.cpp's setLatency: pdc_delay = N - 1
    uint32_t latency() const override { return uint32_t(nullnr::NoiseReducer::latency()); }

private:
    void update()
    {
        // Threshold as the JSFX slider holds it (min + k * 0.5 in double)
        const double thr = -10.0 + std::round((double(fV[3]) + 10.0) / 0.5) * 0.5;
        fDsp.setParams(fV[0] >= 0.5f, fV[1] >= 0.5f, std::round(fV[2]), thr, std::round(fV[4]), std::round(fV[5]),
                       std::round(fV[6]), fV[7] >= 0.5f, std::round(fV[8]));
    }

    nullnr::NoiseReducer fDsp;
    float fV[nullnr::kNumSliders];
};

inline nulljsfx::Module* create() { return new NoiseReductionModule(); }

static const nulljsfx::ModuleDesc kDesc = { "noise_reduction", "Noise Reduction", "Advanced", nullnr::kSliders,
                                            nullnr::kNumSliders, create };

} // namespace noise_reduction
} // namespace nullrack
