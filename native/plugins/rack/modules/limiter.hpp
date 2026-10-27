/*
 * NULL JSFX - Limiter as a Rack module: drives nulllimiter::LimiterProcessor as plugins/limiter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../limiter/LimiterDSP.hpp"
#include "../../limiter/LimiterSliders.hpp"

#include <cmath>

namespace nullrack {
namespace limiter {

class LimiterModule final : public nulljsfx::Module {
public:
    LimiterModule()
    {
        for (uint32_t i = 0; i < nulllimiter::kNumSliders; ++i)
            fV[i] = nulllimiter::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulllimiter::kNumSliders)
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

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay = look

private:
    void update()
    {
        fDsp.setParams(int(std::lround(fV[0])), fV[1], fV[2], fV[3], fV[4], fV[5] >= 0.5f, fV[6] >= 0.5f);
    }

    nulllimiter::LimiterProcessor fDsp;
    float fV[nulllimiter::kNumSliders];
};

inline nulljsfx::Module* create() { return new LimiterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "limiter", "Limiter", "Essentials", nulllimiter::kSliders,
                                            nulllimiter::kNumSliders, create };

} // namespace limiter
} // namespace nullrack
