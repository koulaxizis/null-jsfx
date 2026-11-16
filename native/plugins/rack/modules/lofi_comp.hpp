/*
 * NULL JSFX - Lo-Fi Comp as a Rack module: drives nulllofi::LofiCompProcessor as plugins/lofi_comp/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../lofi_comp/LofiCompDSP.hpp"
#include "../../lofi_comp/LofiCompSliders.hpp"

namespace nullrack {
namespace lofi_comp {

class LofiCompModule final : public nulljsfx::Module {
public:
    LofiCompModule()
    {
        for (uint32_t i = 0; i < nulllofi::kNumSliders; ++i)
            fV[i] = nulllofi::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fSr = sr;
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulllofi::kNumSliders)
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
        double v[nulllofi::kNumSliders];
        for (uint32_t i = 0; i < nulllofi::kNumSliders; ++i)
            v[i] = DISTRHO::jsfxValue(nulllofi::kSliders[i], fV[i]);
        fDsp.setParams(nulllofi::paramsFor(v, fSr));
    }

    nulllofi::LofiCompProcessor fDsp;
    float fV[nulllofi::kNumSliders];
    double fSr = 44100.0;
};

inline nulljsfx::Module* create() { return new LofiCompModule(); }

static const nulljsfx::ModuleDesc kDesc = { "lofi_comp", "Lo-Fi Comp", "Essentials", nulllofi::kSliders,
                                            nulllofi::kNumSliders, create };

} // namespace lofi_comp
} // namespace nullrack
