/*
 * NULL JSFX - De-Plosive as a Rack module: drives nulldeplosive::DePlosiveProcessor as plugins/de_plosive/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../de_plosive/DePlosiveDSP.hpp"
#include "../../de_plosive/DePlosiveSliders.hpp"

#include <cmath>

namespace nullrack {
namespace de_plosive {

class DePlosiveModule final : public nulljsfx::Module {
public:
    DePlosiveModule()
    {
        for (uint32_t i = 0; i < nulldeplosive::kNumSliders; ++i)
            fV[i] = nulldeplosive::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.init(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldeplosive::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.block();
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay = 5 ms

private:
    double v(uint32_t i) const { return nulldeplosive::sliderValue(i, fV[i]); }
    void update()
    {
        nulldeplosive::Params p;
        p.sensitivity = v(0);
        p.frequency = v(1);
        p.maxReduction = v(2);
        p.release = v(3);
        p.listen = int(std::lround(v(4)));
        fDsp.setParams(p);
    }

    nulldeplosive::DePlosiveProcessor fDsp;
    float fV[nulldeplosive::kNumSliders];
};

inline nulljsfx::Module* create() { return new DePlosiveModule(); }

static const nulljsfx::ModuleDesc kDesc = { "de_plosive", "De-Plosive", "Essentials", nulldeplosive::kSliders,
                                            nulldeplosive::kNumSliders, create };

} // namespace de_plosive
} // namespace nullrack
