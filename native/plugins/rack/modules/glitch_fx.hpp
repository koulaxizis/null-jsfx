/*
 * NULL JSFX - GlitchFX as a Rack module: drives nullglitch::GlitchProcessor as plugins/glitch_fx/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../glitch_fx/GlitchFxDSP.hpp"
#include "../../glitch_fx/GlitchFxSliders.hpp"

namespace nullrack {
namespace glitch_fx {

class GlitchFxModule final : public nulljsfx::Module {
public:
    GlitchFxModule()
    {
        for (uint32_t i = 0; i < nullglitch::kNumSliders; ++i)
            fV[i] = nullglitch::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullglitch::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when it reports one, else 120
        const double tempo = time.valid && time.tempo > 0.0 ? time.tempo : 120.0;
        fDsp.process(inL, inR, outL, outR, frames, tempo);
    }

private:
    void update()
    {
        fDsp.setParams(nullglitch::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullglitch::GlitchProcessor fDsp;
    float fV[nullglitch::kNumSliders];
};

inline nulljsfx::Module* create() { return new GlitchFxModule(); }

static const nulljsfx::ModuleDesc kDesc = { "glitch_fx", "Glitch FX", "Advanced", nullglitch::kSliders,
                                            nullglitch::kNumSliders, create };

} // namespace glitch_fx
} // namespace nullrack
