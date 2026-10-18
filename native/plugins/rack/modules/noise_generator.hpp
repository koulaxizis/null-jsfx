/*
 * NULL JSFX - NoiseGenerator as a Rack module: drives nullnoise::NoiseProcessor as plugins/noise_generator/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../noise_generator/NoiseGeneratorDSP.hpp"
#include "../../noise_generator/NoiseGeneratorSliders.hpp"

#include <cmath>

namespace nullrack {
namespace noise_generator {

class NoiseGeneratorModule final : public nulljsfx::Module {
public:
    NoiseGeneratorModule()
    {
        for (uint32_t i = 0; i < nullnoise::kNumSliders; ++i)
            fV[i] = nullnoise::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullnoise::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // as Plugin.cpp: the input feeds Follow Input, Ducking and Mix Mode Add
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    void update()
    {
        fDsp.setParams(int(std::lround(fV[0])), fV[1], fV[2], fV[3], fV[4], fV[5], fV[6], int(std::lround(fV[7])));
    }

    nullnoise::NoiseProcessor fDsp;
    float fV[nullnoise::kNumSliders];
};

inline nulljsfx::Module* create() { return new NoiseGeneratorModule(); }

static const nulljsfx::ModuleDesc kDesc = { "noise_generator", "Noise Generator", "Essentials", nullnoise::kSliders,
                                            nullnoise::kNumSliders, create };

} // namespace noise_generator
} // namespace nullrack
