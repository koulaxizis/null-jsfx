/*
 * NULL JSFX - Hit Layer as a Rack module: drives nullhitlayer::HitLayerProcessor as plugins/hit_layer/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../hit_layer/HitLayerDSP.hpp"
#include "../../hit_layer/HitLayerSliders.hpp"

#include <cmath>

namespace nullrack {
namespace hit_layer {

class HitLayerModule final : public nulljsfx::Module {
public:
    HitLayerModule()
    {
        for (uint32_t i = 0; i < nullhitlayer::kNumSliders; ++i)
            fV[i] = nullhitlayer::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullhitlayer::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // per sample, as Plugin.cpp's process() (its meters and the HIT lamp are display only)
        for (uint32_t i = 0; i < frames; ++i) {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    void update()
    {
        double s[nullhitlayer::kNumSliders];
        for (uint32_t i = 0; i < nullhitlayer::kNumSliders; ++i)
            s[i] = fV[i];
        s[0] = std::round(s[0] * 10.0) / 10.0;
        s[4] = std::round(s[4] * 10.0) / 10.0;
        s[11] = std::round(s[11] * 10.0) / 10.0;
        fDsp.setParams(s);
    }

    nullhitlayer::HitLayerProcessor fDsp;
    float fV[nullhitlayer::kNumSliders];
};

inline nulljsfx::Module* create() { return new HitLayerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "hit_layer", "Hit Layer", "Essentials", nullhitlayer::kSliders,
                                            nullhitlayer::kNumSliders, create };

} // namespace hit_layer
} // namespace nullrack
