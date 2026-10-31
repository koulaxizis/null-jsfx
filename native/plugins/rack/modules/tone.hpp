/*
 * NULL JSFX - Tone as a Rack module: drives nulltone::ToneProcessor as plugins/tone/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../tone/ToneDSP.hpp"
#include "../../tone/ToneSliders.hpp"

namespace nullrack {
namespace tone {

class ToneModule final : public nulljsfx::Module {
public:
    ToneModule()
    {
        for (uint32_t i = 0; i < nulltone::kNumSliders; ++i)
            fV[i] = nulltone::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltone::kNumSliders)
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
    double v(uint32_t i) const { return nulltone::sliderValue(i, fV[i]); }
    void update()
    {
        nulltone::Params p;
        p.bass = v(0);
        p.treble = v(1);
        p.bassFreq = v(2);
        p.trebleFreq = v(3);
        p.output = v(4);
        fDsp.setParams(p);
    }

    nulltone::ToneProcessor fDsp;
    float fV[nulltone::kNumSliders];
};

inline nulljsfx::Module* create() { return new ToneModule(); }

static const nulljsfx::ModuleDesc kDesc = { "tone", "Tone", "Essentials", nulltone::kSliders,
                                            nulltone::kNumSliders, create };

} // namespace tone
} // namespace nullrack
