/*
 * NULL JSFX - Click Repair: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullclick {

// slider1..slider2 of click_repair.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Window", "window", 1.0f, 50.0f, 10.0f, "samples", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullclick
