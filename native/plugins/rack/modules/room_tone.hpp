/*
 * NULL JSFX - RoomTone as a Rack module: drives nullrt::RoomTone as plugins/room_tone/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../room_tone/RoomToneDSP.hpp"
#include "../../room_tone/RoomToneSliders.hpp"

namespace nullrack {
namespace room_tone {

class RoomToneModule final : public nulljsfx::Module {
public:
    RoomToneModule()
    {
        for (uint32_t i = 0; i < nullrt::kNumSliders; ++i)
            fV[i] = nullrt::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // like the JSFX's @init: the learnt profile survives unless the sample rate changes
        fDsp.setSampleRate(sr);
        nullrt::apply(fDsp, fV);
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullrt::kNumSliders)
            return;
        fV[index] = v;
        nullrt::apply(fDsp, fV);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    nullrt::RoomTone fDsp;
    float fV[nullrt::kNumSliders];
};

inline nulljsfx::Module* create() { return new RoomToneModule(); }

static const nulljsfx::ModuleDesc kDesc = { "room_tone", "Room Tone", "Essentials", nullrt::kSliders,
                                            nullrt::kNumSliders, create };

} // namespace room_tone
} // namespace nullrack
