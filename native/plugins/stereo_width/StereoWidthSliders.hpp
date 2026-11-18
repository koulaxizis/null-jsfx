/*
 * NULL JSFX - Stereo Width: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullstereowidth {

// slider1..slider5 of stereo_width.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Width", "width", 0.0f, 200.0f, 100.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Crossover", "crossover", 0.0f, 500.0f, 0.0f, "Hz", 1.0f },
    { "High Crossover", "high_crossover", 0.0f, 16000.0f, 0.0f, "Hz", 10.0f },
    { "High Width", "high_width", 0.0f, 200.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullstereowidth
