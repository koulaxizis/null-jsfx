/*
 * NULL JSFX - Music Box: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullmusicbox {

// slider1..slider9 of music_box.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Decay", "decay", 25.0f, 400.0f, 100.0f, "%", 1.0f },
    { "Brightness", "brightness", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Body", "body", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Box Size", "box_size", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mechanism", "mechanism", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Wind Down", "wind_down", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Width", "width", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullmusicbox
