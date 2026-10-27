/*
 * NULL JSFX - Click Repair: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullclick {

static const char* const kModeOptions[] = { "CLICK", "CRACKLE" };

// slider1..slider3 of click_repair.jsfx, in order (3 appended in 2.7)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Window", "window", 1.0f, 50.0f, 10.0f, "samples", 1.0f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullclick
