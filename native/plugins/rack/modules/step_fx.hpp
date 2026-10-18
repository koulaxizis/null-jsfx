/*
 * NULL JSFX - Step FX as a Rack module: drives nullstepfx::StepFxProcessor as plugins/step_fx/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The Rack passes no MIDI to its modules, so MIDI Trigger cannot be played here: leave it Off
 * in the Rack. Set On, the module does what the plugin does with no notes held (passes the
 * input through), which keeps it identical to the JSFX (module_step_fx).
 */

#pragma once

#include "NullModule.hpp"
#include "../../step_fx/StepFxDSP.hpp"
#include "../../step_fx/StepFxSliders.hpp"

namespace nullrack {
namespace step_fx {

class StepFxModule final : public nulljsfx::Module {
public:
    StepFxModule()
    {
        for (uint32_t i = 0; i < nullstepfx::kNumSliders; ++i)
            fV[i] = nullstepfx::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullstepfx::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime& time) override
    {
        // as Plugin.cpp: the host tempo when it reports one, else 120; the beat position only while playing
        const double tempo = time.valid && time.tempo > 0.0 ? time.tempo : 120.0;
        const double beatPos = time.playing ? time.beat : 0.0;
        fDsp.process(inL, inR, outL, outR, frames, tempo, time.playing, beatPos);
    }

private:
    void update()
    {
        fDsp.setParams(nullstepfx::paramsFrom([this](uint32_t i) { return fV[i]; }));
    }

    nullstepfx::StepFxProcessor fDsp;
    float fV[nullstepfx::kNumSliders];
};

inline nulljsfx::Module* create() { return new StepFxModule(); }

static const nulljsfx::ModuleDesc kDesc = { "step_fx", "Step FX", "Advanced", nullstepfx::kSliders,
                                            nullstepfx::kNumSliders, create };

} // namespace step_fx
} // namespace nullrack
