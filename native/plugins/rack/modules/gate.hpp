/*
 * NULL JSFX - Gate as a Rack module: drives nullgate::GateProcessor as plugins/gate/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../gate/GateDSP.hpp"
#include "../../gate/GateSliders.hpp"

namespace nullrack {
namespace gate {

class GateModule final : public nulljsfx::Module {
public:
    GateModule()
    {
        for (uint32_t i = 0; i < nullgate::kNumSliders; ++i)
            fV[i] = nullgate::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullgate::kNumSliders)
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
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4]);
    }

    nullgate::GateProcessor fDsp;
    float fV[nullgate::kNumSliders];
};

inline nulljsfx::Module* create() { return new GateModule(); }

static const nulljsfx::ModuleDesc kDesc = { "gate", "Gate", "Essentials", nullgate::kSliders,
                                            nullgate::kNumSliders, create };

} // namespace gate
} // namespace nullrack
