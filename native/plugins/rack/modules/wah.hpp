/*
 * NULL JSFX - Wah as a Rack module: drives nullwah::WahProcessor as plugins/wah/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../wah/WahDSP.hpp"
#include "../../wah/WahSliders.hpp"

namespace nullrack {
namespace wah {

class WahModule final : public nulljsfx::Module {
public:
    WahModule()
    {
        for (uint32_t i = 0; i < nullwah::kNumSliders; ++i)
            fV[i] = nullwah::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullwah::kNumSliders)
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
    double v(uint32_t i) const { return nullwah::sliderValue(i, fV[i]); }

    void update()
    {
        nullwah::Params p;
        p.pedal = v(0);
        p.mode = v(1);
        p.model = v(2);
        p.resonance = v(3);
        p.sensitivity = v(4);
        p.attack = v(5);
        p.release = v(6);
        p.rate = v(7);
        p.sync = v(8);
        p.mix = v(9);
        p.output = v(10);
        fDsp.setParams(p);
    }

    nullwah::WahProcessor fDsp;
    float fV[nullwah::kNumSliders];
};

inline nulljsfx::Module* create() { return new WahModule(); }

static const nulljsfx::ModuleDesc kDesc = { "wah", "Wah", "Essentials", nullwah::kSliders,
                                            nullwah::kNumSliders, create };

} // namespace wah
} // namespace nullrack
