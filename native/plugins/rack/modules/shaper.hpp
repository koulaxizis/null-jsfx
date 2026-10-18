/*
 * NULL JSFX - Shaper as a Rack module: drives nullshaper::ShaperProcessor as plugins/shaper/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../shaper/ShaperDSP.hpp"
#include "../../shaper/ShaperSliders.hpp"

#include <cmath>

namespace nullrack {
namespace shaper {

class ShaperModule final : public nulljsfx::Module {
public:
    ShaperModule()
    {
        for (uint32_t i = 0; i < nullshaper::kNumSliders; ++i)
            fV[i] = nullshaper::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.init(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullshaper::kNumSliders)
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

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay

private:
    double v(uint32_t i) const { return nullshaper::sliderValue(i, fV[i]); }
    void update()
    {
        nullshaper::Params p;
        p.shape = int(std::lround(v(0)));
        p.drive = v(1);
        p.bias = v(2);
        p.symmetry = int(std::lround(v(3)));
        p.oversampling = int(std::lround(v(4)));
        p.tone = v(5);
        p.mix = v(6);
        p.output = v(7);
        for (uint32_t i = 0; i < 16; ++i)
            p.points[i] = v(nullshaper::kFirstPoint + i);
        fDsp.setParams(p);
    }

    nullshaper::ShaperProcessor fDsp;
    float fV[nullshaper::kNumSliders];
};

inline nulljsfx::Module* create() { return new ShaperModule(); }

static const nulljsfx::ModuleDesc kDesc = { "shaper", "Shaper", "Essentials", nullshaper::kSliders,
                                            nullshaper::kNumSliders, create };

} // namespace shaper
} // namespace nullrack
