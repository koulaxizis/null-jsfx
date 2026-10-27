/*
 * NULL JSFX - Spectral Ducker as a Rack module: drives nullsduck::SpectralDuckerProcessor as
 * plugins/spectral_ducker/Plugin.cpp does. The Rack has no sidechain bus: the key is the rack's
 * input, the dry signal (RackKeyed.hpp), so a Spectral Ducker after a reverb or a pad carves the
 * dry signal's frequencies out of the effect.
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../RackKeyed.hpp"
#include "../../spectral_ducker/SpectralDuckerDSP.hpp"
#include "../../spectral_ducker/SpectralDuckerSliders.hpp"

namespace nullrack {
namespace spectral_ducker {

class SpectralDuckerModule final : public nulljsfx::Module, public rack::KeyedModule {
public:
    SpectralDuckerModule()
    {
        for (uint32_t i = 0; i < nullsduck::kNumSliders; ++i)
            fV[i] = nullsduck::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullsduck::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void setKey(const float* keyL, const float* keyR) override
    {
        fKeyL = keyL;
        fKeyR = keyR;
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // without a key from the Rack (the module test), the input is the key
        const float* kl = fKeyL ? fKeyL : inL;
        const float* kr = fKeyR ? fKeyR : inR;
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            const double k0 = kl[i], k1 = kr[i]; // read before an in-place write
            fDsp.processSample(l, r, k0, k1);
            outL[i] = float(l);
            outR[i] = float(r);
        }
        fKeyL = fKeyR = nullptr;
    }

private:
    void update()
    {
        double v[nullsduck::kNumSliders];
        for (uint32_t i = 0; i < nullsduck::kNumSliders; ++i)
            v[i] = nullsduck::sliderValue(i, fV[i]);
        fDsp.setParams(v);
    }

    nullsduck::SpectralDuckerProcessor fDsp;
    float fV[nullsduck::kNumSliders];
    const float* fKeyL = nullptr;
    const float* fKeyR = nullptr;
};

inline nulljsfx::Module* create() { return new SpectralDuckerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "spectral_ducker", "Spectral Ducker", "Advanced", nullsduck::kSliders,
                                            nullsduck::kNumSliders, create };

} // namespace spectral_ducker
} // namespace nullrack
