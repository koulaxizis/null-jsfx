/*
 * NULL JSFX - Phase Distortion as a Rack module: drives nullphasedist::PhaseDistortionProcessor as
 * plugins/phase_distortion/Plugin.cpp does; Source Sidechain listens to the Rack's key
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../RackKeyed.hpp"
#include "../../phase_distortion/PhaseDistortionDSP.hpp"
#include "../../phase_distortion/PhaseDistortionSliders.hpp"

namespace nullrack {
namespace phase_distortion {

class PhaseDistortionModule final : public nulljsfx::Module, public rack::KeyedModule {
public:
    PhaseDistortionModule()
    {
        for (uint32_t i = 0; i < nullphasedist::kNumSliders; ++i)
            fV[i] = nullphasedist::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fSr = sr;
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullphasedist::kNumSliders)
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
        fDsp.process(inL, inR, fKeyL ? fKeyL : inL, fKeyR ? fKeyR : inR, outL, outR, frames);
        fKeyL = fKeyR = nullptr;
    }

private:
    void update()
    {
        using nullphasedist::kSliders;
        double v[nullphasedist::kNumSliders];
        for (uint32_t i = 0; i < nullphasedist::kNumSliders; ++i)
            v[i] = DISTRHO::jsfxValue(kSliders[i], fV[i]);
        fDsp.setParams(nullphasedist::paramsFor(v[0], v[1], v[2], v[3], v[4], v[5], v[6], fSr));
    }

    nullphasedist::PhaseDistortionProcessor fDsp;
    float fV[nullphasedist::kNumSliders];
    double fSr = 44100.0;
    const float* fKeyL = nullptr;
    const float* fKeyR = nullptr;
};

inline nulljsfx::Module* create() { return new PhaseDistortionModule(); }

static const nulljsfx::ModuleDesc kDesc = { "phase_distortion", "Phase Distortion", "Advanced", nullphasedist::kSliders,
                                            nullphasedist::kNumSliders, create };

} // namespace phase_distortion
} // namespace nullrack
