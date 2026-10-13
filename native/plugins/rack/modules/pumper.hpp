/*
 * NULL JSFX - Pumper as a Rack module: drives nullpumper::PumperProcessor as plugins/pumper/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../pumper/PumperDSP.hpp"

namespace nullrack {
namespace pumper {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "" },
};

class PumperModule final : public nulljsfx::Module {
public:
    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
        fDsp.setAmount(fAmount);
    }

    void setValue(uint32_t index, float v) override
    {
        if (index != 0)
            return;
        fAmount = v;
        fDsp.setAmount(v);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when above 0, else 120; the beat position only while playing
        // (time.beat is the bar/beat position Plugin.cpp computes from the host's BBT)
        const double tempo = time.tempo > 0.0 ? time.tempo : 120.0;
        const double beatPos = time.playing ? time.beat : 0.0;
        fDsp.process(inL, inR, outL, outR, frames, time.playing, beatPos, tempo);
    }

private:
    nullpumper::PumperProcessor fDsp;
    float fAmount = 50.0f;
};

inline nulljsfx::Module* create() { return new PumperModule(); }

static const nulljsfx::ModuleDesc kDesc = { "pumper", "Pumper", "One Slider", kSliders, 1, create };

} // namespace pumper
} // namespace nullrack
