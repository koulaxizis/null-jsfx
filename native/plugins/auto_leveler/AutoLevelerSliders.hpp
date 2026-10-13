/*
 * NULL JSFX - Auto-Leveler: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullautoleveler {

// slider1..slider5 of auto_leveler.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Target", "target", -36.0f, -6.0f, -18.0f, "dB", 0.5f },
    { "Speed", "speed", 100.0f, 5000.0f, 1000.0f, "ms", 10.0f },
    { "Max Boost", "max_boost", 0.0f, 24.0f, 12.0f, "dB", 0.5f },
    { "Max Cut", "max_cut", 0.0f, 24.0f, 12.0f, "dB", 0.5f },
    { "Gate", "gate", -80.0f, -30.0f, -50.0f, "dB", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullautoleveler
