/*
 * NULL JSFX - Bloom Delay as a Rack module: drives nullbloom::BloomDelayProcessor as plugins/bloom_delay/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../bloom_delay/BloomDelayDSP.hpp"
#include "../../bloom_delay/BloomDelaySliders.hpp"

namespace nullrack {
namespace bloom_delay {

class BloomDelayModule final : public nulljsfx::Module {
public:
    BloomDelayModule()
    {
        for (uint32_t i = 0; i < nullbloom::kNumSliders; ++i)
            fV[i] = nullbloom::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullbloom::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when above 0, else 120
        fDsp.process(inL, inR, outL, outR, frames, time.tempo > 0.0 ? time.tempo : 120.0);
    }

private:
    void update()
    {
        double v[nullbloom::kNumSliders];
        for (uint32_t i = 0; i < nullbloom::kNumSliders; ++i)
            v[i] = DISTRHO::jsfxValue(nullbloom::kSliders[i], fV[i]);
        fDsp.setParams(v);
    }

    nullbloom::BloomDelayProcessor fDsp;
    float fV[nullbloom::kNumSliders];
};

inline nulljsfx::Module* create() { return new BloomDelayModule(); }

static const nulljsfx::ModuleDesc kDesc = { "bloom_delay", "Bloom Delay", "Advanced", nullbloom::kSliders,
                                            nullbloom::kNumSliders, create };

} // namespace bloom_delay
} // namespace nullrack
