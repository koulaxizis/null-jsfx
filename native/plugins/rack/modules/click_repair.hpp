/*
 * NULL JSFX - ClickRepair as a Rack module: drives nullclick::ClickRepairProcessor as plugins/click_repair/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../click_repair/ClickRepairDSP.hpp"
#include "../../click_repair/ClickRepairSliders.hpp"

namespace nullrack {
namespace click_repair {

class ClickRepairModule final : public nulljsfx::Module {
public:
    ClickRepairModule()
    {
        for (uint32_t i = 0; i < nullclick::kNumSliders; ++i)
            fV[i] = nullclick::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullclick::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // one sample at a time, as Plugin.cpp's run()
        for (uint32_t i = 0; i < frames; ++i)
            fDsp.process(inL + i, inR + i, outL + i, outR + i, 1);
    }

    // the JSFX's pdc_delay, as Plugin.cpp's setLatency
    uint32_t latency() const override { return uint32_t(fDsp.latency()); }

private:
    void update() { fDsp.setParams(fV[0], fV[1], fV[2]); }

    nullclick::ClickRepairProcessor fDsp;
    float fV[nullclick::kNumSliders];
};

inline nulljsfx::Module* create() { return new ClickRepairModule(); }

static const nulljsfx::ModuleDesc kDesc = { "click_repair", "Click Repair", "Essentials", nullclick::kSliders,
                                            nullclick::kNumSliders, create };

} // namespace click_repair
} // namespace nullrack
