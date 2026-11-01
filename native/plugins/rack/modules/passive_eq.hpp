/*
 * NULL JSFX - Passive EQ as a Rack module: drives nullpassiveeq::PassiveEqProcessor as plugins/passive_eq/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../passive_eq/PassiveEqDSP.hpp"
#include "../../passive_eq/PassiveEqSliders.hpp"

namespace nullrack {
namespace passive_eq {

class PassiveEqModule final : public nulljsfx::Module {
public:
    PassiveEqModule()
    {
        for (uint32_t i = 0; i < nullpassiveeq::kNumSliders; ++i)
            fV[i] = nullpassiveeq::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullpassiveeq::kNumSliders)
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
        fDsp.setParams(nullpassiveeq::paramsFrom([this](uint32_t i) { return nullpassiveeq::sliderValue(i, fV[i]); }));
    }

    nullpassiveeq::PassiveEqProcessor fDsp;
    float fV[nullpassiveeq::kNumSliders];
};

inline nulljsfx::Module* create() { return new PassiveEqModule(); }

static const nulljsfx::ModuleDesc kDesc = { "passive_eq", "Passive EQ", "Essentials", nullpassiveeq::kSliders,
                                            nullpassiveeq::kNumSliders, create };

} // namespace passive_eq
} // namespace nullrack
