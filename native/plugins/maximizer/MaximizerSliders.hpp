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

// slider1..slider12 of maximizer.jsfx, in order (9..12 appended in 2.4)
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
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's short dither names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5],
    { "Dither", "dither", 0.0f, 2.0f, 0.0f, "", 1.0f, kDitherShort },
    kSliders[7], kSliders[8], kSliders[9], kSliders[10], kSliders[11],
};

} // namespace nullmaximizer
