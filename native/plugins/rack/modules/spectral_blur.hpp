/*
 * NULL JSFX - Spectral Blur as a Rack module: drives nullblur::BlurProcessor as plugins/spectral_blur/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../spectral_blur/SpectralBlurDSP.hpp"
#include "../../spectral_blur/SpectralBlurSliders.hpp"

namespace nullrack {
namespace spectral_blur {

class SpectralBlurModule final : public nulljsfx::Module {
public:
    SpectralBlurModule()
    {
        for (uint32_t i = 0; i < nullblur::kNumSliders; ++i)
            fV[i] = nullblur::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullblur::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    uint32_t latency() const override { return nullblur::BlurProcessor::latency(); } // pdc_delay

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    void update()
    {
        double v[nullblur::kNumSliders];
        for (uint32_t i = 0; i < nullblur::kNumSliders; ++i)
            v[i] = DISTRHO::jsfxValue(nullblur::kSliders[i], fV[i]);
        fDsp.setParams(v);
    }

    nullblur::BlurProcessor fDsp;
    float fV[nullblur::kNumSliders];
};

inline nulljsfx::Module* create() { return new SpectralBlurModule(); }

static const nulljsfx::ModuleDesc kDesc = { "spectral_blur", "Spectral Blur", "Advanced", nullblur::kSliders,
                                            nullblur::kNumSliders, create };

} // namespace spectral_blur
} // namespace nullrack
