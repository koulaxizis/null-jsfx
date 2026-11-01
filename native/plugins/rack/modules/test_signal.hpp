/*
 * NULL JSFX - Test Signal as a Rack module: drives nulltestsignal::TestSignalProcessor as plugins/test_signal/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../test_signal/TestSignalDSP.hpp"
#include "../../test_signal/TestSignalSliders.hpp"

namespace nullrack {
namespace test_signal {

class TestSignalModule final : public nulljsfx::Module {
public:
    TestSignalModule()
    {
        for (uint32_t i = 0; i < nulltestsignal::kNumSliders; ++i)
            fV[i] = nulltestsignal::kSliders[i].def;
        update();
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nulltestsignal::kNumSliders)
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
        fDsp.setParams(nulltestsignal::paramsFrom([this](uint32_t i) { return nulltestsignal::sliderValue(i, fV[i]); }));
    }

    nulltestsignal::TestSignalProcessor fDsp;
    float fV[nulltestsignal::kNumSliders];
};

inline nulljsfx::Module* create() { return new TestSignalModule(); }

static const nulljsfx::ModuleDesc kDesc = { "test_signal", "Test Signal", "Essentials", nulltestsignal::kSliders,
                                            nulltestsignal::kNumSliders, create };

} // namespace test_signal
} // namespace nullrack
