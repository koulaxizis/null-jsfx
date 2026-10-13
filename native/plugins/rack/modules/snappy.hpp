/*
 * NULL JSFX - Snappy as a Rack module: drives nullsnappy::SnappyProcessor as plugins/snappy/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../snappy/SnappyDSP.hpp"

namespace nullrack {
namespace snappy {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class SnappyModule final : public nulljsfx::Module {
public:
    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.setAmount(fAmount);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index != 0)
            return;
        fAmount = v;
        fDsp.setAmount(v);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // per sample, as Plugin.cpp's run() (its scope and gain taps are display only)
        for (uint32_t i = 0; i < frames; ++i) {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    nullsnappy::SnappyProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new SnappyModule(); }

static const nulljsfx::ModuleDesc kDesc = { "snappy", "Snappy", "One Slider", kSliders, 1, create };

} // namespace snappy
} // namespace nullrack
