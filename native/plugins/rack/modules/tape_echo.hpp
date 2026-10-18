/*
 * NULL JSFX - Tape Echo as a Rack module: drives nulltapeecho::TapeEchoProcessor as plugins/tape_echo/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../tape_echo/TapeEchoDSP.hpp"
#include "../../tape_echo/TapeEchoSliders.hpp"

namespace nullrack {
namespace tape_echo {

class TapeEchoModule final : public nulljsfx::Module {
public:
    TapeEchoModule()
    {
        for (uint32_t i = 0; i < nulltapeecho::kNumSliders; ++i)
            fV[i] = nulltapeecho::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltapeecho::kNumSliders)
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
    double v(uint32_t i) const { return nulltapeecho::sliderValue(i, fV[i]); }

    void update()
    {
        nulltapeecho::Params p;
        p.time = v(0);
        p.sync = v(1);
        p.heads = v(2);
        p.intensity = v(3);
        p.bass = v(4);
        p.treble = v(5);
        p.wowFlutter = v(6);
        p.saturation = v(7);
        p.spring = v(8);
        p.mix = v(9);
        p.freeze = v(10);
        fDsp.setParams(p);
    }

    nulltapeecho::TapeEchoProcessor fDsp;
    float fV[nulltapeecho::kNumSliders];
};

inline nulljsfx::Module* create() { return new TapeEchoModule(); }

static const nulljsfx::ModuleDesc kDesc = { "tape_echo", "Tape Echo", "Essentials", nulltapeecho::kSliders,
                                            nulltapeecho::kNumSliders, create };

} // namespace tape_echo
} // namespace nullrack
