/*
 * NULL JSFX - MidSideMatrix as a Rack module: drives nullmidside::MidSideProcessor as plugins/mid_side_matrix/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../mid_side_matrix/MidSideMatrixDSP.hpp"
#include "../../mid_side_matrix/MidSideMatrixSliders.hpp"

#include <cmath>

namespace nullrack {
namespace mid_side_matrix {

class MidSideMatrixModule final : public nulljsfx::Module {
public:
    MidSideMatrixModule()
    {
        for (uint32_t i = 0; i < nullmidside::kNumSliders; ++i)
            fV[i] = nullmidside::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        (void)sr; // as Plugin.cpp: the DSP has no rate
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullmidside::kNumSliders)
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
        fDsp.setParams(nullmidside::tenth(fV[0]), nullmidside::tenth(fV[1]), double(std::lround(fV[2])));
    }

    nullmidside::MidSideProcessor fDsp;
    float fV[nullmidside::kNumSliders];
};

inline nulljsfx::Module* create() { return new MidSideMatrixModule(); }

static const nulljsfx::ModuleDesc kDesc = { "mid_side_matrix", "Mid/Side Matrix", "Essentials", nullmidside::kSliders,
                                            nullmidside::kNumSliders, create };

} // namespace mid_side_matrix
} // namespace nullrack
