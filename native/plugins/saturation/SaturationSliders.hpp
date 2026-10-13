/*
 * NULL JSFX - Saturation: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullsaturation {

// the host sees the JSFX's mode names; the selector in the interface shows short ones
static const char* const kModeOptions[] = { "Tape", "Tube", "Overdrive", "Fuzz", "Bitcrush", "Waveshaper", "Fold" };
static const char* const kModeShort[] = { "TAPE", "TUBE", "DRIVE", "FUZZ", "CRUSH", "SHAPE", "FOLD" };

// slider1..slider4 of saturation.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Drive", "drive", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Tone", "tone", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mode", "mode", 0.0f, 6.0f, 0.0f, "", 1.0f, kModeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's short mode names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2],
    { "Mode", "mode", 0.0f, 6.0f, 0.0f, "", 1.0f, kModeShort },
};

} // namespace nullsaturation
