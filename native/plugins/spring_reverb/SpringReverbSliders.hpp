/*
 * NULL JSFX - Spring Reverb: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullspring {

// slider1..slider6 of spring_reverb.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Decay", "decay", 0.5f, 8.0f, 2.5f, "s", 0.1f },
    { "Tension", "tension", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Tone", "tone", 1000.0f, 8000.0f, 4000.0f, "Hz", 10.0f },
    { "Drip", "drip", 0.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Pre-Delay", "predelay", 0.0f, 100.0f, 0.0f, "ms", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullspring
