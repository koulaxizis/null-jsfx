/*
 * NULL JSFX - Glue Comp as a Rack module: drives nullgluecomp::GlueCompProcessor as plugins/glue_comp/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../glue_comp/GlueCompDSP.hpp"
#include "../../glue_comp/GlueCompSliders.hpp"

namespace nullrack {
namespace glue_comp {

class GlueCompModule final : public nulljsfx::Module {
public:
    GlueCompModule()
    {
        for (uint32_t i = 0; i < nullgluecomp::kNumSliders; ++i)
            fV[i] = nullgluecomp::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullgluecomp::kNumSliders)
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
    double v(uint32_t i) const { return nullgluecomp::sliderValue(i, fV[i]); }
    void update()
    {
        nullgluecomp::Params p;
        p.threshold = v(0);
        p.ratio = int(v(1));
        p.attack = int(v(2));
        p.release = int(v(3));
        p.makeup = v(4);
        p.mix = v(5);
        p.hpf = v(6);
        fDsp.setParams(p);
    }

    nullgluecomp::GlueCompProcessor fDsp;
    float fV[nullgluecomp::kNumSliders];
};

inline nulljsfx::Module* create() { return new GlueCompModule(); }

static const nulljsfx::ModuleDesc kDesc = { "glue_comp", "Glue Comp", "Essentials", nullgluecomp::kSliders,
                                            nullgluecomp::kNumSliders, create };

} // namespace glue_comp
} // namespace nullrack
