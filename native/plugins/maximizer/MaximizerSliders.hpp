/*
 * NULL JSFX - Maximizer: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullmaximizer {

static const char* const kLinkOptions[] = { "OFF", "ON" };

// slider1..slider6 of maximizer.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Input Gain", "input", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Ceiling", "ceiling", -3.0f, -0.1f, -0.3f, "dB", 0.1f },
    { "Release", "release", 10.0f, 200.0f, 50.0f, "ms", 1.0f },
    { "Lookahead", "lookahead", 0.0f, 5.0f, 2.0f, "ms", 1.0f },
    { "Output Gain", "output", -12.0f, 0.0f, 0.0f, "dB", 0.1f },
    { "Stereo Link", "link", 0.0f, 1.0f, 1.0f, "", 1.0f, kLinkOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullmaximizer
