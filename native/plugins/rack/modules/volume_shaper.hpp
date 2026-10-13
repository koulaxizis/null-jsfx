/*
 * NULL JSFX - VolumeShaper as a Rack module: drives nullvolshaper::VolumeShaperProcessor as plugins/volume_shaper/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../volume_shaper/VolumeShaperDSP.hpp"
#include "../../volume_shaper/VolumeShaperSliders.hpp"

namespace nullrack {
namespace volume_shaper {

class VolumeShaperModule final : public nulljsfx::Module {
public:
    VolumeShaperModule()
    {
        for (uint32_t i = 0; i < nullvolshaper::kNumSliders; ++i)
            fV[i] = nullvolshaper::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullvolshaper::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when it reports one, else 120; the beat position only while playing
        const double tempo = time.valid && time.tempo > 0.0 ? time.tempo : 120.0;
        const double beatPos = time.playing ? time.beat : 0.0;
        fDsp.process(inL, inR, outL, outR, frames, time.playing, beatPos, tempo);
    }

private:
    void update()
    {
        fDsp.setParams(nullvolshaper::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullvolshaper::VolumeShaperProcessor fDsp;
    float fV[nullvolshaper::kNumSliders];
};

inline nulljsfx::Module* create() { return new VolumeShaperModule(); }

static const nulljsfx::ModuleDesc kDesc = { "volume_shaper", "Volume Shaper", "Essentials", nullvolshaper::kSliders,
                                            nullvolshaper::kNumSliders, create };

} // namespace volume_shaper
} // namespace nullrack
