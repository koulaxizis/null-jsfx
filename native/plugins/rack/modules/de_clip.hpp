/*
 * NULL JSFX - De-Clip as a Rack module: drives nulldeclip::DeClipProcessor as plugins/de_clip/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../de_clip/DeClipDSP.hpp"
#include "../../de_clip/DeClipSliders.hpp"

namespace nullrack {
namespace de_clip {

class DeClipModule final : public nulljsfx::Module {
public:
    DeClipModule()
    {
        for (uint32_t i = 0; i < nulldeclip::kNumSliders; ++i)
            fV[i] = nulldeclip::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulldeclip::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay

private:
    double v(uint32_t i) const { return nulldeclip::sliderValue(i, fV[i]); }
    void update() { fDsp.setParams(nulldeclip::params(v(0), v(1), v(2), v(3))); }

    nulldeclip::DeClipProcessor fDsp;
    float fV[nulldeclip::kNumSliders];
};

inline nulljsfx::Module* create() { return new DeClipModule(); }

static const nulljsfx::ModuleDesc kDesc = { "de_clip", "De-Clip", "Essentials", nulldeclip::kSliders,
                                            nulldeclip::kNumSliders, create };

} // namespace de_clip
} // namespace nullrack
