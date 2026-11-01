/*
 * NULL JSFX - Multi-Tap Delay as a Rack module: drives nullmultitap::MultiTapProcessor as plugins/multi_tap_delay/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../multi_tap_delay/MultiTapDSP.hpp"
#include "../../multi_tap_delay/MultiTapSliders.hpp"

namespace nullrack {
namespace multi_tap_delay {

class MultiTapDelayModule final : public nulljsfx::Module {
public:
    MultiTapDelayModule()
    {
        for (uint32_t i = 0; i < nullmultitap::kNumSliders; ++i)
            fV[i] = nullmultitap::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullmultitap::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        fDsp.beginBlock(time.valid && time.tempo > 0.0 ? time.tempo : 120.0);
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
        fDsp.setParams(nullmultitap::paramsFrom([this](uint32_t i) { return nullmultitap::sliderValue(i, fV[i]); }));
    }

    nullmultitap::MultiTapProcessor fDsp;
    float fV[nullmultitap::kNumSliders];
};

inline nulljsfx::Module* create() { return new MultiTapDelayModule(); }

static const nulljsfx::ModuleDesc kDesc = { "multi_tap_delay", "Multi-Tap Delay", "Essentials", nullmultitap::kSliders,
                                            nullmultitap::kNumSliders, create };

} // namespace multi_tap_delay
} // namespace nullrack
