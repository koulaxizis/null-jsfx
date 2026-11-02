/*
 * NULL JSFX - Binaural Panner: the JSFX sliders, and their values as the JSFX holds them
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "BinauralPannerDSP.hpp"

namespace nullbp {

static const char* const kInputOptions[] = { "MONO", "STEREO" };

// slider1..slider8 of binaural_panner.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Azimuth", "azimuth", -180.0f, 180.0f, 0.0f, "deg", 1.0f },
    { "Elevation", "elevation", -40.0f, 90.0f, 0.0f, "deg", 1.0f },
    { "Distance", "distance", 0.25f, 20.0f, 1.0f, "m", 0.01f },
    { "Room", "room", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Head Size", "head_size", 14.0f, 20.0f, 17.5f, "cm", 0.1f },
    { "Input", "input", 0.0f, 1.0f, 0.0f, "", 1.0f, kInputOptions },
    { "Width", "width", 0.0f, 90.0f, 30.0f, "deg", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the values as the JSFX sliders hold them
inline void apply(BinauralPanner& dsp, const float* v)
{
    double d[kNumSliders];
    for (uint32_t i = 0; i < kNumSliders; ++i)
        d[i] = DISTRHO::jsfxValue(kSliders[i], v[i]);
    dsp.setParams(d[0], d[1], d[2], d[3], d[4], d[5] >= 0.5, d[6], d[7]);
}

} // namespace nullbp
