/*
 * NULL JSFX - Band Spin as a Rack module: drives nullbandspin::BandSpinProcessor as plugins/band_spin/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../band_spin/BandSpinDSP.hpp"
#include "../../band_spin/BandSpinSliders.hpp"

namespace nullrack {
namespace band_spin {

class BandSpinModule final : public nulljsfx::Module {
public:
    BandSpinModule()
    {
        for (uint32_t i = 0; i < nullbandspin::kNumSliders; ++i)
            fV[i] = nullbandspin::kSliders[i].def;
    }

    // the delay lines are allocated here (the Rack calls it outside process())
    void setSampleRate(double sr) override
    {
        update();
        fDsp.setSampleRate(sr);
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullbandspin::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
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
        double s[nullbandspin::kNumSliders];
        nullbandspin::readSliders(fV, s);
        fDsp.setParams(s);
    }

    nullbandspin::BandSpinProcessor fDsp;
    float fV[nullbandspin::kNumSliders];
};

inline nulljsfx::Module* create() { return new BandSpinModule(); }

static const nulljsfx::ModuleDesc kDesc = { "band_spin", "Band Spin", "Advanced", nullbandspin::kSliders,
                                            nullbandspin::kNumSliders, create };

} // namespace band_spin
} // namespace nullrack
