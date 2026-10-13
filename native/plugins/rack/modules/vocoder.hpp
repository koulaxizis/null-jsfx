/*
 * NULL JSFX - Vocoder as a Rack module: drives nullvocoder::VocoderProcessor as plugins/vocoder/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../vocoder/VocoderDSP.hpp"
#include "../../vocoder/VocoderSliders.hpp"

#include <cmath>

namespace nullrack {
namespace vocoder {

class VocoderModule final : public nulljsfx::Module {
public:
    VocoderModule()
    {
        for (uint32_t i = 0; i < nullvocoder::kNumSliders; ++i)
            fV[i] = nullvocoder::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullvocoder::kNumSliders)
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
    // a 0.1-step parameter as the JSFX slider holds it: min + k * 0.1 in double (the float
    // parameter alone would detune the oscillator, e.g. 396.3 Hz -> 396.29998779 Hz)
    double stepped(uint32_t i) const
    {
        const DISTRHO::SliderInfo& in = nullvocoder::kSliders[i];
        const double step = 1.0 / std::round(1.0 / double(in.step));
        return double(in.min) + std::round((double(fV[i]) - double(in.min)) / step) * step;
    }

    void update()
    {
        fDsp.setParams(int(std::lround(fV[0])), stepped(1), stepped(2), stepped(3), fV[4], fV[5], fV[6],
                       stepped(7), fV[8]);
    }

    nullvocoder::VocoderProcessor fDsp;
    float fV[nullvocoder::kNumSliders];
};

inline nulljsfx::Module* create() { return new VocoderModule(); }

static const nulljsfx::ModuleDesc kDesc = { "vocoder", "Vocoder", "Advanced", nullvocoder::kSliders,
                                            nullvocoder::kNumSliders, create };

} // namespace vocoder
} // namespace nullrack
