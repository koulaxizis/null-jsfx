/*
 * NULL JSFX - Bass Presence as a Rack module: drives nullbasspresence::BassPresenceProcessor as plugins/bass_presence/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../bass_presence/BassPresenceDSP.hpp"

namespace nullrack {
namespace bass_presence {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 40.0f, "" },
};

class BassPresenceModule final : public nulljsfx::Module {
public:
    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
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
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    nullbasspresence::BassPresenceProcessor fDsp;
    float fAmount = 40.0f;
};

inline nulljsfx::Module* create() { return new BassPresenceModule(); }

static const nulljsfx::ModuleDesc kDesc = { "bass_presence", "Bass Presence", "One Slider", kSliders, 1, create };

} // namespace bass_presence
} // namespace nullrack
