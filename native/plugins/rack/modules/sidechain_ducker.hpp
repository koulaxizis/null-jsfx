/*
 * NULL JSFX - Sidechain Ducker as a Rack module: drives nullducker::DuckerProcessor as
 * plugins/sidechain_ducker/Plugin.cpp does. The Rack has no sidechain bus: the key (Key = SIDE)
 * is the rack's input, the dry signal (RackKeyed.hpp), so a ducker after a delay or a reverb ducks
 * the effect while the dry signal plays. Key = MAIN keys on the module's own input, as the plugin.
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../RackKeyed.hpp"
#include "../../sidechain_ducker/SidechainDuckerDSP.hpp"
#include "../../sidechain_ducker/SidechainDuckerSliders.hpp"

#include <cmath>

namespace nullrack {
namespace sidechain_ducker {

class SidechainDuckerModule final : public nulljsfx::Module, public rack::KeyedModule {
public:
    SidechainDuckerModule()
    {
        for (uint32_t i = 0; i < nullducker::kNumSliders; ++i)
            fV[i] = nullducker::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullducker::kNumSliders)
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
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4], fV[5] >= 0.5f, fV[6]);
    }

    nullducker::DuckerProcessor fDsp;
    float fV[nullducker::kNumSliders];
    const float* fKeyL = nullptr;
    const float* fKeyR = nullptr;
};

inline nulljsfx::Module* create() { return new SidechainDuckerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "sidechain_ducker", "Sidechain Ducker", "Essentials", nullducker::kSliders,
                                            nullducker::kNumSliders, create };

} // namespace sidechain_ducker
} // namespace nullrack
