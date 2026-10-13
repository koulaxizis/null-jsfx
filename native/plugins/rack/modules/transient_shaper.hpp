/*
 * NULL JSFX - Transient Shaper as a Rack module: drives nulltransient::TransientProcessor as plugins/transient_shaper/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../transient_shaper/TransientShaperDSP.hpp"
#include "../../transient_shaper/TransientShaperSliders.hpp"

namespace nullrack {
namespace transient_shaper {

class TransientShaperModule final : public nulljsfx::Module {
public:
    TransientShaperModule()
    {
        for (uint32_t i = 0; i < nulltransient::kNumSliders; ++i)
            fV[i] = nulltransient::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltransient::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // per sample, as Plugin.cpp's run() (its meters and scope are display only)
        for (uint32_t i = 0; i < frames; ++i) {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

private:
    void update()
    {
        fDsp.setParams(fV[0], fV[1], fV[2]);
    }

    nulltransient::TransientProcessor fDsp;
    float fV[nulltransient::kNumSliders];
};

inline nulljsfx::Module* create() { return new TransientShaperModule(); }

static const nulljsfx::ModuleDesc kDesc = { "transient_shaper", "Transient Shaper", "Essentials", nulltransient::kSliders,
                                            nulltransient::kNumSliders, create };

} // namespace transient_shaper
} // namespace nullrack
