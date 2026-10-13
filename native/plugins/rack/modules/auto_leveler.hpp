/*
 * NULL JSFX - Auto-Leveler as a Rack module: drives nullautoleveler::AutoLevelerProcessor as plugins/auto_leveler/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../auto_leveler/AutoLevelerDSP.hpp"
#include "../../auto_leveler/AutoLevelerSliders.hpp"

namespace nullrack {
namespace auto_leveler {

class AutoLevelerModule final : public nulljsfx::Module {
public:
    AutoLevelerModule()
    {
        for (uint32_t i = 0; i < nullautoleveler::kNumSliders; ++i)
            fV[i] = nullautoleveler::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullautoleveler::kNumSliders)
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
    void update() { fDsp.setParams(fV[0], fV[1], fV[2], fV[3], fV[4]); }

    nullautoleveler::AutoLevelerProcessor fDsp;
    float fV[nullautoleveler::kNumSliders];
};

inline nulljsfx::Module* create() { return new AutoLevelerModule(); }

static const nulljsfx::ModuleDesc kDesc = { "auto_leveler", "Auto-Leveler", "Essentials", nullautoleveler::kSliders,
                                            nullautoleveler::kNumSliders, create };

} // namespace auto_leveler
} // namespace nullrack
