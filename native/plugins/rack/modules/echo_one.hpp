/*
 * NULL JSFX - Echo One as a Rack module: drives nullechoone::EchoOneProcessor as plugins/echo_one/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../echo_one/EchoOneDSP.hpp"

namespace nullrack {
namespace echo_one {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 40.0f, "" },
};

class EchoOneModule final : public nulljsfx::Module {
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
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when above 0, else 120
        fDsp.process(inL, inR, outL, outR, frames, time.tempo > 0.0 ? time.tempo : 120.0);
    }

private:
    nullechoone::EchoOneProcessor fDsp;
    float fAmount = 40.0f;
};

inline nulljsfx::Module* create() { return new EchoOneModule(); }

static const nulljsfx::ModuleDesc kDesc = { "echo_one", "Echo One", "One Slider", kSliders, 1, create };

} // namespace echo_one
} // namespace nullrack
