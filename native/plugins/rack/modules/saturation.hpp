/*
 * NULL JSFX - Saturation as a Rack module: drives nullsaturation::SaturationProcessor as plugins/saturation/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../saturation/SaturationDSP.hpp"
#include "../../saturation/SaturationSliders.hpp"

#include <cmath>

namespace nullrack {
namespace saturation {

class SaturationModule final : public nulljsfx::Module {
public:
    SaturationModule()
    {
        for (uint32_t i = 0; i < nullsaturation::kNumSliders; ++i)
            fV[i] = nullsaturation::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullsaturation::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay

private:
    void update()
    {
        nullsaturation::Params p;
        p.drive = fV[0];
        p.tone = fV[1];
        p.mix = fV[2];
        p.mode = int(std::lround(fV[3]));
        p.input = fV[4];
        p.output = fV[5];
        p.oversampling = int(std::lround(fV[6]));
        p.emphasis = fV[7];
        fDsp.setParams(p);
    }

    nullsaturation::SaturationProcessor fDsp;
    float fV[nullsaturation::kNumSliders];
};

inline nulljsfx::Module* create() { return new SaturationModule(); }

static const nulljsfx::ModuleDesc kDesc = { "saturation", "Saturation", "Essentials", nullsaturation::kSliders,
                                            nullsaturation::kNumSliders, create };

} // namespace saturation
} // namespace nullrack
