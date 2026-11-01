/*
 * NULL JSFX - Bitcrusher as a Rack module: drives nullbitcrusher::BitcrusherProcessor as plugins/bitcrusher/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../bitcrusher/BitcrusherDSP.hpp"
#include "../../bitcrusher/BitcrusherSliders.hpp"

namespace nullrack {
namespace bitcrusher {

class BitcrusherModule final : public nulljsfx::Module {
public:
    BitcrusherModule()
    {
        for (uint32_t i = 0; i < nullbitcrusher::kNumSliders; ++i)
            fV[i] = nullbitcrusher::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullbitcrusher::kNumSliders)
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

private:
    void update()
    {
        fDsp.setParams(nullbitcrusher::paramsFrom([this](uint32_t i) { return nullbitcrusher::sliderValue(i, fV[i]); }));
    }

    nullbitcrusher::BitcrusherProcessor fDsp;
    float fV[nullbitcrusher::kNumSliders];
};

inline nulljsfx::Module* create() { return new BitcrusherModule(); }

static const nulljsfx::ModuleDesc kDesc = { "bitcrusher", "Bitcrusher", "Essentials", nullbitcrusher::kSliders,
                                            nullbitcrusher::kNumSliders, create };

} // namespace bitcrusher
} // namespace nullrack
