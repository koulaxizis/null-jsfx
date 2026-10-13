/*
 * NULL JSFX - PitchShifter as a Rack module: drives nullpitchshifter::PitchShifterProcessor as plugins/pitch_shifter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../pitch_shifter/PitchShifterDSP.hpp"
#include "../../pitch_shifter/PitchShifterSliders.hpp"

namespace nullrack {
namespace pitch_shifter {

class PitchShifterModule final : public nulljsfx::Module {
public:
    PitchShifterModule()
    {
        for (uint32_t i = 0; i < nullpitchshifter::kNumSliders; ++i)
            fV[i] = nullpitchshifter::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullpitchshifter::kNumSliders)
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
    void update()
    {
        using nullpitchshifter::onGrid;
        fDsp.setParams(fV[0], fV[1], onGrid(fV[2], -50.0, 50.0, 0.1), fV[3]);
    }

    nullpitchshifter::PitchShifterProcessor fDsp;
    float fV[nullpitchshifter::kNumSliders];
};

inline nulljsfx::Module* create() { return new PitchShifterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "pitch_shifter", "Pitch Shifter", "Essentials", nullpitchshifter::kSliders,
                                            nullpitchshifter::kNumSliders, create };

} // namespace pitch_shifter
} // namespace nullrack
