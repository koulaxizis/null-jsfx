/*
 * NULL JSFX - TranceGate as a Rack module: drives nulltrance::TranceGateProcessor as plugins/trance_gate/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../trance_gate/TranceGateDSP.hpp"
#include "../../trance_gate/TranceGateSliders.hpp"

namespace nullrack {
namespace trance_gate {

class TranceGateModule final : public nulljsfx::Module {
public:
    TranceGateModule()
    {
        for (uint32_t i = 0; i < nulltrance::kNumSliders; ++i)
            fV[i] = nulltrance::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltrance::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when it reports one, else 120; the beat position only while playing
        const double tempo = time.valid && time.tempo > 0.0 ? time.tempo : 120.0;
        const double beatPos = time.playing ? time.beat : 0.0;
        fDsp.process(inL, inR, outL, outR, frames, tempo, time.playing, beatPos);
    }

private:
    void update()
    {
        fDsp.setParams(nulltrance::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nulltrance::TranceGateProcessor fDsp;
    float fV[nulltrance::kNumSliders];
};

inline nulljsfx::Module* create() { return new TranceGateModule(); }

static const nulljsfx::ModuleDesc kDesc = { "trance_gate", "Trance Gate", "Advanced", nulltrance::kSliders,
                                            nulltrance::kNumSliders, create };

} // namespace trance_gate
} // namespace nullrack
