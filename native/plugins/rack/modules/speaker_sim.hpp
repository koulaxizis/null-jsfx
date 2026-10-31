/*
 * NULL JSFX - Speaker Sim as a Rack module: drives nullspeakersim::SpeakerSimProcessor as plugins/speaker_sim/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../speaker_sim/SpeakerSimDSP.hpp"
#include "../../speaker_sim/SpeakerSimSliders.hpp"

namespace nullrack {
namespace speaker_sim {

class SpeakerSimModule final : public nulljsfx::Module {
public:
    SpeakerSimModule()
    {
        for (uint32_t i = 0; i < nullspeakersim::kNumSliders; ++i)
            fV[i] = nullspeakersim::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullspeakersim::kNumSliders)
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
        fDsp.setParams(nullspeakersim::paramsFrom([this](uint32_t i) { return nullspeakersim::sliderValue(i, fV[i]); }));
    }

    nullspeakersim::SpeakerSimProcessor fDsp;
    float fV[nullspeakersim::kNumSliders];
};

inline nulljsfx::Module* create() { return new SpeakerSimModule(); }

static const nulljsfx::ModuleDesc kDesc = { "speaker_sim", "Speaker Sim", "Essentials", nullspeakersim::kSliders,
                                            nullspeakersim::kNumSliders, create };

} // namespace speaker_sim
} // namespace nullrack
