/*
 * NULL JSFX - TapeStop as a Rack module: drives nulltapestop::TapeStopProcessor as plugins/tape_stop/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../tape_stop/TapeStopDSP.hpp"
#include "../../tape_stop/TapeStopSliders.hpp"

namespace nullrack {
namespace tape_stop {

class TapeStopModule final : public nulljsfx::Module {
public:
    TapeStopModule()
    {
        for (uint32_t i = 0; i < nulltapestop::kNumSliders; ++i)
            fV[i] = nulltapestop::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltapestop::kNumSliders)
            return;
        fV[index] = v;
        if (index == 0)
            fDsp.setTrigger(this->v(0));
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    double v(uint32_t i) const { return nulltapestop::sliderValue(i, fV[i]); }

    void update()
    {
        fDsp.setParams(v(1), v(2));
    }

    nulltapestop::TapeStopProcessor fDsp;
    float fV[nulltapestop::kNumSliders];
};

inline nulljsfx::Module* create() { return new TapeStopModule(); }

static const nulljsfx::ModuleDesc kDesc = { "tape_stop", "Tape Stop", "Advanced", nulltapestop::kSliders,
                                            nulltapestop::kNumSliders, create };

} // namespace tape_stop
} // namespace nullrack
