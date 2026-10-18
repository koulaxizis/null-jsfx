/*
 * NULL JSFX - Filter as a Rack module: drives nullfilter::FilterProcessor as plugins/filter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../filter/FilterDSP.hpp"
#include "../../filter/FilterSliders.hpp"

#include <cmath>

namespace nullrack {
namespace filter {

class FilterModule final : public nulljsfx::Module {
public:
    FilterModule()
    {
        for (uint32_t i = 0; i < nullfilter::kNumSliders; ++i)
            fV[i] = nullfilter::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullfilter::kNumSliders)
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
        fDsp.process(inL, inR, outL, outR, frames, tempo, time.playing, beatPos);
    }

private:
    void update()
    {
        nullfilter::Params p;
        p.cutoff = fV[0];
        p.resonance = fV[1];
        p.gain = fV[2];
        p.mode = int(std::lround(fV[3]));
        p.slope = int(std::lround(fV[4]));
        p.drive = fV[5];
        p.lfoRate = fV[6];
        p.lfoSync = int(std::lround(fV[7]));
        p.lfoDepth = fV[8];
        p.envDepth = fV[9];
        p.envAttack = fV[10];
        p.envRelease = fV[11];
        fDsp.setParams(p);
    }

    nullfilter::FilterProcessor fDsp;
    float fV[nullfilter::kNumSliders];
};

inline nulljsfx::Module* create() { return new FilterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "filter", "Filter", "Essentials", nullfilter::kSliders,
                                            nullfilter::kNumSliders, create };

} // namespace filter
} // namespace nullrack
