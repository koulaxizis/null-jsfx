/*
 * NULL JSFX - Granular Delay: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullgranular {

// slider1..slider9 of granular_delay.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Delay Time", "time", 10.0f, 2000.0f, 300.0f, "ms", 1.0f },
    { "Grain Size", "size", 10.0f, 500.0f, 80.0f, "ms", 1.0f },
    { "Density", "density", 1.0f, 100.0f, 20.0f, "grains/s", 1.0f },
    { "Pitch", "pitch", -24.0f, 24.0f, 0.0f, "st", 0.1f },
    { "Spray", "spray", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Reverse", "reverse", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Feedback", "feedback", 0.0f, 95.0f, 40.0f, "%", 1.0f },
    { "Stereo Spread", "spread", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 40.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullgranular
