/*
 * NULL JSFX - Breath Control: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullbreath {

// slider1..slider4 of breath_control.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", -70.0f, 0.0f, -40.0f, "dB", 0.5f },
    { "Attack", "attack", 1.0f, 50.0f, 5.0f, "ms", 1.0f },
    { "Release", "release", 20.0f, 1000.0f, 120.0f, "ms", 1.0f },
    { "Range", "range", -24.0f, 0.0f, -12.0f, "dB", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullbreath
