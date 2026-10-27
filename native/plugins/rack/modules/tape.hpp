/*
 * NULL JSFX - Tape as a Rack module: drives nulltape::TapeProcessor as plugins/tape/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../tape/TapeDSP.hpp"
#include "../../tape/TapeSliders.hpp"

namespace nullrack {
namespace tape {

class TapeModule final : public nulljsfx::Module {
public:
    TapeModule()
    {
        for (uint32_t i = 0; i < nulltape::kNumSliders; ++i)
            fV[i] = nulltape::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // @init then @slider
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltape::kNumSliders)
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
        double s[nulltape::kNumSliders];
        for (uint32_t i = 0; i < nulltape::kNumSliders; ++i)
            s[i] = nulltape::sliderValue(i, fV[i]);
        fDsp.setParams(s);
    }

    nulltape::TapeProcessor fDsp;
    float fV[nulltape::kNumSliders];
};

inline nulljsfx::Module* create() { return new TapeModule(); }

static const nulljsfx::ModuleDesc kDesc = { "tape", "Tape", "Essentials", nulltape::kSliders,
                                            nulltape::kNumSliders, create };

} // namespace tape
} // namespace nullrack
