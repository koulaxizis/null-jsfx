/*
 * NULL JSFX - Tremolo as a Rack module: drives nulltremolo::TremoloProcessor as plugins/tremolo/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../tremolo/TremoloDSP.hpp"
#include "../../tremolo/TremoloSliders.hpp"

#include <cmath>

namespace nullrack {
namespace tremolo {

class TremoloModule final : public nulljsfx::Module {
public:
    TremoloModule()
    {
        for (uint32_t i = 0; i < nulltremolo::kNumSliders; ++i)
            fV[i] = nulltremolo::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltremolo::kNumSliders)
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
        using nulltremolo::onGrid;
        fDsp.setParams(onGrid(fV[0], 0.1, 20.0, 0.1), fV[1], fV[2], int(std::lround(fV[3])), int(std::lround(fV[4])),
                       fV[5], fV[6] >= 0.5f);
    }

    nulltremolo::TremoloProcessor fDsp;
    float fV[nulltremolo::kNumSliders];
};

inline nulljsfx::Module* create() { return new TremoloModule(); }

static const nulljsfx::ModuleDesc kDesc = { "tremolo", "Tremolo", "Essentials", nulltremolo::kSliders,
                                            nulltremolo::kNumSliders, create };

} // namespace tremolo
} // namespace nullrack
