/*
 * NULL JSFX - Convolution as a Rack module: drives nullconv::ConvolutionProcessor as
 * plugins/convolution/Plugin.cpp does, with the five built-in rooms only.
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The IR File source needs a file per instance (the plugin's "irfile" state), and the Rack keeps
 * no per-slot files: its Source slider here ends at Cathedral (0..4), so the Rack's knobs,
 * presets and modulation never reach IR File. The values are still the JSFX's sliders in order
 * (slider1, slider3..slider8; the file slider itself is not a value, as in the plugin), so
 * module_convolution can compare against the JSFX. That test also sets Source to 5 (IR File):
 * with no file loaded the DSP builds no IR, exactly like the JSFX with no file chosen.
 */

#pragma once

#include "NullModule.hpp"
#include "../../convolution/ConvolutionDSP.hpp"
#include "../../convolution/ConvolutionSliders.hpp"

#include <cmath>

namespace nullrack {
namespace convolution {

static const char* const kRackSources[] = { "Small Room", "Large Room", "Hall", "Plate", "Cathedral" };

// nullconv::kSliders with Source limited to the built-in rooms
static const DISTRHO::SliderInfo kSliders[] = {
    { "Source", "source", 0.0f, 4.0f, 2.0f, "", 1.0f, kRackSources },
    nullconv::kSliders[1], nullconv::kSliders[2], nullconv::kSliders[3],
    nullconv::kSliders[4], nullconv::kSliders[5], nullconv::kSliders[6],
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);
static_assert(kNumSliders == nullconv::kNumSliders, "Convolution: one module value per plugin parameter");

class ConvolutionModule final : public nulljsfx::Module {
public:
    ConvolutionModule()
    {
        for (uint32_t i = 0; i < kNumSliders; ++i)
            fV[i] = nullconv::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        setParams();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= kNumSliders)
            return;
        fV[index] = v;
        setParams();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        // as Plugin.cpp's run: the JSFX's @slider (an IR rebuild if anything changed), then @sample
        fDsp.update();
        fDsp.process(inL, inR, outL, outR, frames);
    }

    uint32_t latency() const override { return uint32_t(nullconv::ConvolutionProcessor::latency()); }

private:
    void setParams()
    {
        // Output as the JSFX slider holds it (min + k * 0.1 in double)
        const double out = -24.0 + std::round((double(fV[6]) + 24.0) / 0.1) * 0.1;
        fDsp.setParams(int(std::lround(fV[0])), fV[1], fV[2], fV[3], fV[4], fV[5], out);
    }

    nullconv::ConvolutionProcessor fDsp;
    float fV[kNumSliders];
};

inline nulljsfx::Module* create() { return new ConvolutionModule(); }

static const nulljsfx::ModuleDesc kDesc = { "convolution", "Convolution", "Advanced", kSliders, kNumSliders, create };

} // namespace convolution
} // namespace nullrack
