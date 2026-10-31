/*
 * NULL JSFX - Console Buss as a Rack module: drives nullconsole::ConsoleProcessor as plugins/console_buss/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../console_channel/ConsoleDSP.hpp"
#include "../../console_channel/ConsoleSliders.hpp"

namespace nullrack {
namespace console_buss {

class ConsoleBussModule final : public nulljsfx::Module {
public:
    ConsoleBussModule()
        : fDsp(true)
    {
        for (uint32_t i = 0; i < nullconsole::kNumSliders; ++i)
            fV[i] = nullconsole::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullconsole::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    double v(uint32_t i) const { return nullconsole::sliderValue(i, fV[i]); }
    void update() { fDsp.setParams(int(v(0)), v(1), v(2)); }

    nullconsole::ConsoleProcessor fDsp;
    float fV[nullconsole::kNumSliders];
};

inline nulljsfx::Module* create() { return new ConsoleBussModule(); }

static const nulljsfx::ModuleDesc kDesc = { "console_buss", "Console Buss", "Advanced", nullconsole::kSliders,
                                            nullconsole::kNumSliders, create };

} // namespace console_buss
} // namespace nullrack
