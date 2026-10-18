/*
 * NULL JSFX - Slicer as a Rack module: drives nullslicer::SlicerProcessor as plugins/slicer/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../slicer/SlicerDSP.hpp"
#include "../../slicer/SlicerSliders.hpp"

namespace nullrack {
namespace slicer {

class SlicerModule final : public nulljsfx::Module {
public:
    SlicerModule()
    {
        for (uint32_t i = 0; i < nullslicer::kNumSliders; ++i)
            fV[i] = nullslicer::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullslicer::kNumSliders)
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
        fDsp.setParams(nullslicer::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullslicer::SlicerProcessor fDsp;
    float fV[nullslicer::kNumSliders];
    bool fWasPlaying = false;
};

inline nulljsfx::Module* create() { return new SlicerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "slicer", "Slicer", "Advanced", nullslicer::kSliders,
                                            nullslicer::kNumSliders, create };

} // namespace slicer
} // namespace nullrack
