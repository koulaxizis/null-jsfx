/*
 * NULL JSFX - MultibandComp as a Rack module: drives nullmultiband::MultibandProcessor as plugins/multiband_compressor/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../multiband_compressor/MultibandCompressorDSP.hpp"
#include "../../multiband_compressor/MultibandCompressorSliders.hpp"

#include <cmath>

namespace nullrack {
namespace multiband_compressor {

class MultibandCompressorModule final : public nulljsfx::Module {
public:
    MultibandCompressorModule()
    {
        for (uint32_t i = 0; i < nullmultiband::kNumSliders; ++i)
            fV[i] = nullmultiband::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        update();
        fDsp.setSampleRate(sr);
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullmultiband::kNumSliders)
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
        nullmultiband::Params p;
        for (int k = 0; k < 3; ++k)
            p.xover[k] = std::round(fV[k]);
        for (int b = 0; b < 4; ++b)
        {
            const uint32_t o = 3 + 4 * uint32_t(b);
            p.band[b].thr = std::round(fV[o]);
            p.band[b].ratio = nullmultiband::tenth(fV[o + 1]);
            p.band[b].makeupDb = std::round(fV[o + 2]);
            p.band[b].mode = int(std::lround(fV[o + 3]));
        }
        p.attackMs = std::round(fV[19]);
        p.releaseMs = std::round(fV[20]);
        p.kneePct = std::round(fV[21]);
        p.masterDb = std::round(fV[22]);
        fDsp.setParams(p);
    }

    nullmultiband::MultibandProcessor fDsp;
    float fV[nullmultiband::kNumSliders];
};

inline nulljsfx::Module* create() { return new MultibandCompressorModule(); }

static const nulljsfx::ModuleDesc kDesc = { "multiband_compressor", "Multiband Comp", "Essentials", nullmultiband::kSliders,
                                            nullmultiband::kNumSliders, create };

} // namespace multiband_compressor
} // namespace nullrack
