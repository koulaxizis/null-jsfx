/*
 * NULL JSFX - Maximizer: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullmaximizer {

static const char* const kLinkOptions[] = { "OFF", "ON" };
static const char* const kDitherOptions[] = { "OFF", "16-BIT", "24-BIT" };
static const char* const kDitherShort[] = { "OFF", "16 BIT", "24 BIT" };
static const char* const kCharacterOptions[] = { "CLEAN", "PUNCHY", "WARM" };
static const char* const kListenOptions[] = { "OUTPUT", "DELTA" };
static const char* const kLearnOptions[] = { "OFF", "ON" };

// slider1..slider16 of maximizer.jsfx, in order (9..12 appended in 2.4, 13..14 in 2.7, 15..16 in 2.9)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Input Gain", "input", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Ceiling", "ceiling", -3.0f, -0.1f, -0.3f, "dB", 0.1f },
    { "Release", "release", 10.0f, 200.0f, 50.0f, "ms", 1.0f },
    { "Lookahead", "lookahead", 0.0f, 5.0f, 2.0f, "ms", 1.0f },
    { "Output Gain", "output", -12.0f, 0.0f, 0.0f, "dB", 0.1f },
    { "Stereo Link", "link", 0.0f, 1.0f, 1.0f, "", 1.0f, kLinkOptions },
    { "Dither", "dither", 0.0f, 2.0f, 0.0f, "", 1.0f, kDitherOptions },
    { "Noise Shaping", "shaping", 0.0f, 1.0f, 0.0f, "", 1.0f, kLinkOptions },
    { "Character", "character", 0.0f, 2.0f, 0.0f, "", 1.0f, kCharacterOptions },
    { "Multiband", "multiband", 0.0f, 1.0f, 0.0f, "", 1.0f, kLinkOptions },
    { "Low Crossover", "low_xover", 50.0f, 500.0f, 150.0f, "Hz", 10.0f },
    { "High Crossover", "high_xover", 1000.0f, 12000.0f, 4000.0f, "Hz", 100.0f },
    { "True Peak", "true_peak", 0.0f, 1.0f, 0.0f, "", 1.0f, kLinkOptions },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
    { "Target", "target", -24.0f, -6.0f, -14.0f, "LUFS", 0.5f },
    { "Learn", "learn", 0.0f, 1.0f, 0.0f, "", 1.0f, kLearnOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's short dither names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5],
    { "Dither", "dither", 0.0f, 2.0f, 0.0f, "", 1.0f, kDitherShort },
    kSliders[7], kSliders[8], kSliders[9], kSliders[10], kSliders[11], kSliders[12], kSliders[13],
    kSliders[14], kSliders[15],
};

// the JSFX's double for a host float (NullSliders.hpp): Input Gain -2.3 is -2.3, not the float's
// -2.2999999523, so the 16-bit dither rounds the same samples as the JSFX
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullmaximizer
