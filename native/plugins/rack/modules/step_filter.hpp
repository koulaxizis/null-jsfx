/*
 * NULL JSFX - StepFilter as a Rack module: drives nullstepfilter::StepFilterProcessor as plugins/step_filter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../step_filter/StepFilterDSP.hpp"
#include "../../step_filter/StepFilterSliders.hpp"

namespace nullrack {
namespace step_filter {

class StepFilterModule final : public nulljsfx::Module {
public:
    StepFilterModule()
    {
        for (uint32_t i = 0; i < nullstepfilter::kNumSliders; ++i)
            fV[i] = nullstepfilter::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullstepfilter::kNumSliders)
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
        fDsp.setParams(nullstepfilter::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullstepfilter::StepFilterProcessor fDsp;
    float fV[nullstepfilter::kNumSliders];
};

inline nulljsfx::Module* create() { return new StepFilterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "step_filter", "Step Filter", "Essentials", nullstepfilter::kSliders,
                                            nullstepfilter::kNumSliders, create };

} // namespace step_filter
} // namespace nullrack
