/*
 * NULL JSFX - Vocoder: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullvocoder {

static const char* const kCarrierOptions[] = { "RIGHT", "SAW", "PULSE", "NOISE" };

// slider1..slider9 of vocoder.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Carrier", "carrier", 0.0f, 3.0f, 1.0f, "", 1.0f, kCarrierOptions },
    { "Carrier Pitch", "pitch", 40.0f, 880.0f, 110.0f, "Hz", 0.1f },
    { "Formant Shift", "formant", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Attack", "attack", 0.5f, 50.0f, 5.0f, "ms", 0.1f },
    { "Release", "release", 5.0f, 500.0f, 40.0f, "ms", 1.0f },
    { "Sibilance", "sibilance", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Stereo Width", "width", 0.0f, 100.0f, 60.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullvocoder
