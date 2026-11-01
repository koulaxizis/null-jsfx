/*
 * NULL JSFX - Flanger as a Rack module: drives nullflanger::FlangerProcessor as plugins/flanger/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../flanger/FlangerDSP.hpp"
#include "../../flanger/FlangerSliders.hpp"

namespace nullrack {
namespace flanger {

class FlangerModule final : public nulljsfx::Module {
public:
    FlangerModule()
    {
        for (uint32_t i = 0; i < nullflanger::kNumSliders; ++i)
            fV[i] = nullflanger::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullflanger::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when above 0, else 120; the beat position only while playing
        const double tempo = time.tempo > 0.0 ? time.tempo : 120.0;
        const double beatPos = time.playing ? time.beat : 0.0;
        fDsp.process(inL, inR, outL, outR, frames, time.playing, beatPos, tempo);
    }

    // the JSFX's pdc_delay, as Plugin.cpp's setLatency (only while Thru Zero is on)
    uint32_t latency() const override { return uint32_t(fDsp.latency()); }

private:
    void update()
    {
        using nullflanger::onGrid;
        const int sync = int(std::lround(fV[5]));
        fDsp.setParams(onGrid(fV[0], 0.1, 5.0, 0.1), onGrid(fV[1], 1.0, 20.0, 0.1), fV[2], fV[3],
                       onGrid(fV[4], 0.1, 10.0, 0.1), sync, nullflanger::syncCycle(sync), std::round(fV[6]),
                       fV[7] >= 0.5f, int(std::lround(fV[8])));
    }

    nullflanger::FlangerProcessor fDsp;
    float fV[nullflanger::kNumSliders];
};

inline nulljsfx::Module* create() { return new FlangerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "flanger", "Flanger", "Essentials", nullflanger::kSliders,
                                            nullflanger::kNumSliders, create };

} // namespace flanger
} // namespace nullrack
