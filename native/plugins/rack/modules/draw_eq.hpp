/*
 * NULL JSFX - Draw EQ as a Rack module: drives nulldraweq::DrawEqProcessor as plugins/draw_eq/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../draw_eq/DrawEqDSP.hpp"
#include "../../draw_eq/DrawEqSliders.hpp"

namespace nullrack {
namespace draw_eq {

class DrawEqModule final : public nulljsfx::Module {
public:
    DrawEqModule()
    {
        for (uint32_t i = 0; i < nulldraweq::kNumSliders; ++i)
            fV[i] = nulldraweq::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldraweq::kNumSliders)
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
        nulldraweq::Params p;
        for (int i = 0; i < nulldraweq::kPoints; ++i)
            p.gain[i] = nulldraweq::sliderValue(uint32_t(i), fV[i]);
        p.smooth = nulldraweq::sliderValue(nulldraweq::kSmoothness, fV[nulldraweq::kSmoothness]);
        p.output = nulldraweq::sliderValue(nulldraweq::kOutput, fV[nulldraweq::kOutput]);
        p.mix = nulldraweq::sliderValue(nulldraweq::kMix, fV[nulldraweq::kMix]);
        fDsp.setParams(p);
    }

    nulldraweq::DrawEqProcessor fDsp;
    float fV[nulldraweq::kNumSliders];
};

inline nulljsfx::Module* create() { return new DrawEqModule(); }

static const nulljsfx::ModuleDesc kDesc = { "draw_eq", "Draw EQ", "Essentials", nulldraweq::kSliders,
                                            nulldraweq::kNumSliders, create };

} // namespace draw_eq
} // namespace nullrack
