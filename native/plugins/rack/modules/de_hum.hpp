/*
 * NULL JSFX - DeHum as a Rack module: drives nulldehum::DeHumProcessor as plugins/de_hum/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../de_hum/DeHumDSP.hpp"
#include "../../de_hum/DeHumSliders.hpp"

#include <cmath>

namespace nullrack {
namespace de_hum {

class DeHumModule final : public nulljsfx::Module {
public:
    DeHumModule()
    {
        for (uint32_t i = 0; i < nulldehum::kNumSliders; ++i)
            fV[i] = nulldehum::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.init(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldehum::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    double v(uint32_t i) const { return nulldehum::sliderValue(i, fV[i]); }
    void update()
    {
        nulldehum::Params p;
        p.mains = int(std::lround(v(0)));
        p.harmonics = v(1);
        p.q = v(2);
        p.depth = v(3);
        p.fine = v(4);
        p.lowCut = int(std::lround(v(5)));
        fDsp.setParams(p);
    }

    nulldehum::DeHumProcessor fDsp;
    float fV[nulldehum::kNumSliders];
};

inline nulljsfx::Module* create() { return new DeHumModule(); }

static const nulljsfx::ModuleDesc kDesc = { "de_hum", "De-Hum", "Essentials", nulldehum::kSliders,
                                            nulldehum::kNumSliders, create };

} // namespace de_hum
} // namespace nullrack
