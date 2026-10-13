/*
 * NULL JSFX - DeEsser as a Rack module: drives nulldeesser::DeEsserProcessor as plugins/deesser/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../deesser/DeEsserDSP.hpp"
#include "../../deesser/DeEsserSliders.hpp"

namespace nullrack {
namespace deesser {

class DeesserModule final : public nulljsfx::Module {
public:
    DeesserModule()
    {
        for (uint32_t i = 0; i < nulldeesser::kNumSliders; ++i)
            fV[i] = nulldeesser::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldeesser::kNumSliders)
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
    void update() { fDsp.setParams(fV[0], fV[1]); }

    nulldeesser::DeEsserProcessor fDsp;
    float fV[nulldeesser::kNumSliders];
};

inline nulljsfx::Module* create() { return new DeesserModule(); }

static const nulljsfx::ModuleDesc kDesc = { "deesser", "DeEsser", "Essentials", nulldeesser::kSliders,
                                            nulldeesser::kNumSliders, create };

} // namespace deesser
} // namespace nullrack
