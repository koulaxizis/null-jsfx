/*
 * NULL JSFX - Envelope Follower as a Rack module: drives nullenvfollower::EnvelopeFollowerProcessor as plugins/envelope_follower/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../envelope_follower/EnvelopeFollowerDSP.hpp"
#include "../../envelope_follower/EnvelopeFollowerSliders.hpp"

#include <cmath>

namespace nullrack {
namespace envelope_follower {

class EnvelopeFollowerModule final : public nulljsfx::Module {
public:
    EnvelopeFollowerModule()
    {
        for (uint32_t i = 0; i < nullenvfollower::kNumSliders; ++i)
            fV[i] = nullenvfollower::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullenvfollower::kNumSliders)
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

private:
    void update()
    {
        fDsp.setParams(int(std::lround(fV[0])), fV[1], fV[2], fV[3], fV[4], fV[5], fV[6], fV[7] >= 0.5f, fV[8]);
    }

    nullenvfollower::EnvelopeFollowerProcessor fDsp;
    float fV[nullenvfollower::kNumSliders];
};

inline nulljsfx::Module* create() { return new EnvelopeFollowerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "envelope_follower", "Envelope Follower", "Essentials", nullenvfollower::kSliders,
                                            nullenvfollower::kNumSliders, create };

} // namespace envelope_follower
} // namespace nullrack
