/*
 * NULL JSFX - Half Speed as a Rack module: drives nullhalfspeed::HalfSpeedProcessor as plugins/half_speed/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../half_speed/HalfSpeedDSP.hpp"
#include "../../half_speed/HalfSpeedSliders.hpp"

namespace nullrack {
namespace half_speed {

class HalfSpeedModule final : public nulljsfx::Module {
public:
    HalfSpeedModule()
    {
        for (uint32_t i = 0; i < nullhalfspeed::kNumSliders; ++i)
            fV[i] = nullhalfspeed::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullhalfspeed::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when it reports one, else 120; the beat position only
        // while playing, and a fresh start (the JSFX's @init) when the transport starts
        const double tempo = time.valid && time.tempo > 0.0 ? time.tempo : 120.0;
        const double beatPos = time.playing ? time.beat : 0.0;
        if (time.playing && !fWasPlaying)
            fDsp.reset();
        fWasPlaying = time.playing;
        fDsp.process(inL, inR, outL, outR, frames, tempo, time.playing, beatPos);
    }

private:
    void update()
    {
        fDsp.setParams(nullhalfspeed::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullhalfspeed::HalfSpeedProcessor fDsp;
    float fV[nullhalfspeed::kNumSliders];
    bool fWasPlaying = false;
};

inline nulljsfx::Module* create() { return new HalfSpeedModule(); }

static const nulljsfx::ModuleDesc kDesc = { "half_speed", "Half Speed", "Advanced", nullhalfspeed::kSliders,
                                            nullhalfspeed::kNumSliders, create };

} // namespace half_speed
} // namespace nullrack
