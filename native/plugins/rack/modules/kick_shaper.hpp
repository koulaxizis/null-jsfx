/*
 * NULL JSFX - KickShaper as a Rack module: drives nullkick::KickShaperProcessor as plugins/kick_shaper/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../kick_shaper/KickShaperDSP.hpp"
#include "../../kick_shaper/KickShaperSliders.hpp"

namespace nullrack {
namespace kick_shaper {

class KickShaperModule final : public nulljsfx::Module {
public:
    KickShaperModule()
    {
        for (uint32_t i = 0; i < nullkick::kNumSliders; ++i)
            fV[i] = nullkick::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullkick::kNumSliders)
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
        fDsp.setParams(nullkick::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullkick::KickShaperProcessor fDsp;
    float fV[nullkick::kNumSliders];
};

inline nulljsfx::Module* create() { return new KickShaperModule(); }

static const nulljsfx::ModuleDesc kDesc = { "kick_shaper", "Kick Shaper", "Advanced", nullkick::kSliders,
                                            nullkick::kNumSliders, create };

} // namespace kick_shaper
} // namespace nullrack
