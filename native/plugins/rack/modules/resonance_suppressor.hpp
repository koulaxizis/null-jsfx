/*
 * NULL JSFX - ResonanceSuppressor as a Rack module: drives nullrsp::ResonanceSuppressorProcessor as plugins/resonance_suppressor/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../resonance_suppressor/ResonanceSuppressorDSP.hpp"
#include "../../resonance_suppressor/ResonanceSuppressorSliders.hpp"

#include <cmath>

namespace nullrack {
namespace resonance_suppressor {

class ResonanceSuppressorModule final : public nulljsfx::Module {
public:
    ResonanceSuppressorModule()
    {
        for (uint32_t i = 0; i < nullrsp::kNumSliders; ++i)
            fV[i] = nullrsp::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullrsp::kNumSliders)
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
    uint32_t latency() const override { return uint32_t(nullrsp::ResonanceSuppressorProcessor::latency()); }

private:
    void update()
    {
        // Depth as the JSFX slider holds it (min + k * 0.1 in double)
        const double depth = std::round(double(fV[0]) / 0.1) * 0.1;
        fDsp.setParams(depth, fV[1], fV[2], fV[3], fV[4], fV[5], fV[6], fV[7] >= 0.5f, fV[8] >= 0.5f, fV[9]);
    }

    nullrsp::ResonanceSuppressorProcessor fDsp;
    float fV[nullrsp::kNumSliders];
};

inline nulljsfx::Module* create() { return new ResonanceSuppressorModule(); }

static const nulljsfx::ModuleDesc kDesc = { "resonance_suppressor", "Resonance Suppressor", "Advanced", nullrsp::kSliders,
                                            nullrsp::kNumSliders, create };

} // namespace resonance_suppressor
} // namespace nullrack
