/*
 * NULL JSFX - Dynamics: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulldynamics {

static const char* const kModeOptions[] = { "COMP", "EXPAND" };

// slider1..slider7 of dynamics.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Threshold", "threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
    { "Ratio", "ratio", 1.0f, 20.0f, 4.0f, "", 0.1f },
    { "Knee", "knee", 0.0f, 20.0f, 6.0f, "dB", 1.0f },
    { "Attack", "attack", 0.0f, 200.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 0.0f, 1000.0f, 100.0f, "ms", 1.0f },
    { "Makeup", "makeup", 0.0f, 20.0f, 0.0f, "dB", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulldynamics
