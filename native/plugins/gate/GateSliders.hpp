/*
 * NULL JSFX - Gate: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullgate {

// slider1..slider5 of gate.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", -60.0f, 0.0f, -40.0f, "dB", 0.1f },
    { "Attack", "attack", 1.0f, 100.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 50.0f, 1000.0f, 200.0f, "ms", 1.0f },
    { "Sidechain HPF", "sc_hpf", 0.0f, 500.0f, 0.0f, "Hz", 1.0f },
    { "Range", "range", -80.0f, 0.0f, -80.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullgate
