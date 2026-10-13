/*
 * NULL JSFX - FormantFilter as a Rack module: drives nullformant::FormantProcessor as plugins/formant_filter/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../formant_filter/FormantFilterDSP.hpp"
#include "../../formant_filter/FormantFilterSliders.hpp"

#include <cmath>

namespace nullrack {
namespace formant_filter {

class FormantFilterModule final : public nulljsfx::Module {
public:
    FormantFilterModule()
    {
        for (uint32_t i = 0; i < nullformant::kNumSliders; ++i)
            fV[i] = nullformant::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullformant::kNumSliders)
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
    double v(uint32_t i) const { return nullformant::sliderValue(i, fV[i]); }

    void update()
    {
        fDsp.setParams(int(std::lround(v(0))), v(1), v(2));
    }

    nullformant::FormantProcessor fDsp;
    float fV[nullformant::kNumSliders];
};

inline nulljsfx::Module* create() { return new FormantFilterModule(); }

static const nulljsfx::ModuleDesc kDesc = { "formant_filter", "Formant Filter", "Advanced", nullformant::kSliders,
                                            nullformant::kNumSliders, create };

} // namespace formant_filter
} // namespace nullrack
