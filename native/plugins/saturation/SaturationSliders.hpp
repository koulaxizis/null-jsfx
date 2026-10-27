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
static const char* const kOsOptions[] = { "Off", "2x", "4x" };
static const char* const kOsShort[] = { "OFF", "2X", "4X" };
static const char* const kChannelsOptions[] = { "STEREO", "MID", "SIDE" };

// slider1..slider9 of saturation.jsfx, in order (9 appended in 2.7)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Drive", "drive", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Tone", "tone", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mode", "mode", 0.0f, 6.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Input", "input", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Oversampling", "oversampling", 0.0f, 2.0f, 0.0f, "", 1.0f, kOsOptions },
    { "Emphasis", "emphasis", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Channels", "channels", 0.0f, 2.0f, 0.0f, "", 1.0f, kChannelsOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's short mode names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2],
    { "Mode", "mode", 0.0f, 6.0f, 0.0f, "", 1.0f, kModeShort },
    kSliders[4], kSliders[5],
    { "Oversampling", "oversampling", 0.0f, 2.0f, 0.0f, "", 1.0f, kOsShort },
    kSliders[7], kSliders[8],
};

} // namespace nullsaturation
