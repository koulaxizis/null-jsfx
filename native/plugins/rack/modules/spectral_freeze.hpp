/*
 * NULL JSFX - SpectralFreeze as a Rack module: drives nullfreeze::FreezeProcessor as plugins/spectral_freeze/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../spectral_freeze/SpectralFreezeDSP.hpp"
#include "../../spectral_freeze/SpectralFreezeSliders.hpp"

#include <cmath>

namespace nullrack {
namespace spectral_freeze {

class SpectralFreezeModule final : public nulljsfx::Module {
public:
    SpectralFreezeModule()
    {
        for (uint32_t i = 0; i < nullfreeze::kNumSliders; ++i)
            fV[i] = nullfreeze::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullfreeze::kNumSliders)
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
    void update()
    {
        // Level as the JSFX slider holds it (min + k * 0.1 in double)
        const double level = -24.0 + std::round((double(fV[3]) + 24.0) / 0.1) * 0.1;
        fDsp.setParams(fV[0] >= 0.5f, fV[1], fV[2], level, fV[4]);
    }

    nullfreeze::FreezeProcessor fDsp;
    float fV[nullfreeze::kNumSliders];
};

inline nulljsfx::Module* create() { return new SpectralFreezeModule(); }

static const nulljsfx::ModuleDesc kDesc = { "spectral_freeze", "Spectral Freeze", "Advanced", nullfreeze::kSliders,
                                            nullfreeze::kNumSliders, create };

} // namespace spectral_freeze
} // namespace nullrack
