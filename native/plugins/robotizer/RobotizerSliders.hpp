/*
 * NULL JSFX - Robotizer: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullrobotizer {

// slider1..slider4 of robotizer.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Pitch", "pitch", 50.0f, 400.0f, 120.0f, "Hz", 1.0f },
    { "Metallic", "metallic", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Ring", "ring", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullrobotizer
