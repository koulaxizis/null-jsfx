/*
 * NULL JSFX - Crossfeed as a Rack module: drives nullxfeed::CrossfeedProcessor as plugins/crossfeed/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../crossfeed/CrossfeedDSP.hpp"
#include "../../crossfeed/CrossfeedSliders.hpp"

namespace nullrack {
namespace crossfeed {

class CrossfeedModule final : public nulljsfx::Module {
public:
    CrossfeedModule()
    {
        for (uint32_t i = 0; i < nullxfeed::kNumSliders; ++i)
            fV[i] = nullxfeed::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullxfeed::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    void update()
    {
        double v[nullxfeed::kNumSliders];
        for (uint32_t i = 0; i < nullxfeed::kNumSliders; ++i)
            v[i] = nullxfeed::sliderValue(i, fV[i]);
        fDsp.setParams(nullxfeed::params(v));
    }

    nullxfeed::CrossfeedProcessor fDsp;
    float fV[nullxfeed::kNumSliders];
};

inline nulljsfx::Module* create() { return new CrossfeedModule(); }

static const nulljsfx::ModuleDesc kDesc = { "crossfeed", "Crossfeed", "Essentials", nullxfeed::kSliders,
                                            nullxfeed::kNumSliders, create };

} // namespace crossfeed
} // namespace nullrack
