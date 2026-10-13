/*
 * NULL JSFX - PitchCorrection as a Rack module: drives nullpitchcorrection::PitchCorrectionProcessor as plugins/pitch_correction/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../pitch_correction/PitchCorrectionDSP.hpp"
#include "../../pitch_correction/PitchCorrectionSliders.hpp"

#include <cmath>

namespace nullrack {
namespace pitch_correction {

class PitchCorrectionModule final : public nulljsfx::Module {
public:
    PitchCorrectionModule()
    {
        for (uint32_t i = 0; i < nullpitchcorrection::kNumSliders; ++i)
            fV[i] = nullpitchcorrection::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullpitchcorrection::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // as Plugin.cpp's run(), without its pitch-history display
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

    // as Plugin.cpp's setLatency: pdc_delay = lat
    uint32_t latency() const override { return fDsp.latency(); }

private:
    void update()
    {
        fDsp.setParams(int(std::lround(fV[0])), int(std::lround(fV[1])), fV[2], fV[3], double(std::lround(fV[4])), fV[5],
                       fV[6]);
    }

    nullpitchcorrection::PitchCorrectionProcessor fDsp;
    float fV[nullpitchcorrection::kNumSliders];
};

inline nulljsfx::Module* create() { return new PitchCorrectionModule(); }

static const nulljsfx::ModuleDesc kDesc = { "pitch_correction", "Pitch Correction", "Advanced", nullpitchcorrection::kSliders,
                                            nullpitchcorrection::kNumSliders, create };

} // namespace pitch_correction
} // namespace nullrack
