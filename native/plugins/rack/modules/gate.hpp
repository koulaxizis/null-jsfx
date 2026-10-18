/*
 * NULL JSFX - Gate as a Rack module: drives nullgate::GateProcessor as plugins/gate/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The Rack's modules are stereo, so this one has no sidechain input: the detector always listens
 * to the slot's own signal and Sidechain = External acts like Internal (the JSFX's External with
 * the track on pins 3/4).
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

    // the JSFX's pdc_delay, as Plugin.cpp's setLatency (only while Lookahead is above 0)
    uint32_t latency() const override { return uint32_t(fDsp.latency()); }

private:
    void update()
    {
        fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4], fV[5], nullgate::onGrid(fV[6]), false, fV[8],
                       fV[9] >= 0.5f); // External: no sidechain bus in the Rack, treated as Internal
    }

    nullgate::GateProcessor fDsp;
    float fV[nullgate::kNumSliders];
};

inline nulljsfx::Module* create() { return new GateModule(); }

static const nulljsfx::ModuleDesc kDesc = { "gate", "Gate", "Essentials", nullgate::kSliders,
                                            nullgate::kNumSliders, create };

} // namespace gate
} // namespace nullrack
