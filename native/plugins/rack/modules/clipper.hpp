/*
 * NULL JSFX - Clipper as a Rack module: drives nullclipper::ClipperProcessor as plugins/clipper/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../clipper/ClipperDSP.hpp"
#include "../../clipper/ClipperSliders.hpp"

#include <cmath>

namespace nullrack {
namespace clipper {

class ClipperModule final : public nulljsfx::Module {
public:
    ClipperModule()
    {
        for (uint32_t i = 0; i < nullclipper::kNumSliders; ++i)
            fV[i] = nullclipper::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.init(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullclipper::kNumSliders)
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
    double v(uint32_t i) const { return nullclipper::sliderValue(i, fV[i]); }
    void update()
    {
        nullclipper::Params p;
        p.input = v(0);
        p.ceiling = v(1);
        p.knee = v(2);
        p.oversampling = int(std::lround(v(3)));
        p.mix = v(4);
        p.output = v(5);
        p.link = int(std::lround(v(6)));
        p.delta = v(7) >= 0.5f;
        fDsp.setParams(p);
    }

    nullclipper::ClipperProcessor fDsp;
    float fV[nullclipper::kNumSliders];
};

inline nulljsfx::Module* create() { return new ClipperModule(); }

static const nulljsfx::ModuleDesc kDesc = { "clipper", "Clipper", "Essentials", nullclipper::kSliders,
                                            nullclipper::kNumSliders, create };

} // namespace clipper
} // namespace nullrack
