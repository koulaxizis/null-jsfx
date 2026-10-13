/*
 * NULL JSFX - RotarySpeaker as a Rack module: drives nullrotary::RotaryProcessor as plugins/rotary_speaker/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../rotary_speaker/RotarySpeakerDSP.hpp"
#include "../../rotary_speaker/RotarySpeakerSliders.hpp"

namespace nullrack {
namespace rotary_speaker {

class RotarySpeakerModule final : public nulljsfx::Module {
public:
    RotarySpeakerModule()
    {
        for (uint32_t i = 0; i < nullrotary::kNumSliders; ++i)
            fV[i] = nullrotary::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullrotary::kNumSliders)
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
        fDsp.setParams(nullrotary::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullrotary::RotaryProcessor fDsp;
    float fV[nullrotary::kNumSliders];
};

inline nulljsfx::Module* create() { return new RotarySpeakerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "rotary_speaker", "Rotary Speaker", "Advanced", nullrotary::kSliders,
                                            nullrotary::kNumSliders, create };

} // namespace rotary_speaker
} // namespace nullrack
