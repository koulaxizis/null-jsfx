/*
 * NULL JSFX - Disperser as a Rack module: drives nulldisperser::DisperserProcessor as plugins/disperser/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../disperser/DisperserDSP.hpp"
#include "../../disperser/DisperserSliders.hpp"

namespace nullrack {
namespace disperser {

class DisperserModule final : public nulljsfx::Module {
public:
    DisperserModule()
    {
        for (uint32_t i = 0; i < nulldisperser::kNumSliders; ++i)
            fV[i] = nulldisperser::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.init(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldisperser::kNumSliders)
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
    double v(uint32_t i) const { return nulldisperser::sliderValue(i, fV[i]); }
    void update()
    {
        nulldisperser::Params p;
        p.freq = v(0);
        p.amount = v(1);
        p.pinch = v(2);
        p.mix = v(3);
        p.mode = int(v(4));
        fDsp.setParams(p);
    }

    nulldisperser::DisperserProcessor fDsp;
    float fV[nulldisperser::kNumSliders];
};

inline nulljsfx::Module* create() { return new DisperserModule(); }

static const nulljsfx::ModuleDesc kDesc = { "disperser", "Disperser", "Essentials", nulldisperser::kSliders,
                                            nulldisperser::kNumSliders, create };

} // namespace disperser
} // namespace nullrack
