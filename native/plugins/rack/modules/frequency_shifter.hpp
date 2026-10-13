/*
 * NULL JSFX - FrequencyShifter as a Rack module: drives nullfreqshift::FreqShiftProcessor as plugins/frequency_shifter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../frequency_shifter/FrequencyShifterDSP.hpp"
#include "../../frequency_shifter/FrequencyShifterSliders.hpp"

namespace nullrack {
namespace frequency_shifter {

class FrequencyShifterModule final : public nulljsfx::Module {
public:
    FrequencyShifterModule()
    {
        for (uint32_t i = 0; i < nullfreqshift::kNumSliders; ++i)
            fV[i] = nullfreqshift::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullfreqshift::kNumSliders)
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
    double v(uint32_t i) const { return nullfreqshift::sliderValue(i, fV[i]); }

    void update()
    {
        fDsp.setParams(v(0), v(1), v(2), v(3));
    }

    nullfreqshift::FreqShiftProcessor fDsp;
    float fV[nullfreqshift::kNumSliders];
};

inline nulljsfx::Module* create() { return new FrequencyShifterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "frequency_shifter", "Frequency Shifter", "Advanced", nullfreqshift::kSliders,
                                            nullfreqshift::kNumSliders, create };

} // namespace frequency_shifter
} // namespace nullrack
