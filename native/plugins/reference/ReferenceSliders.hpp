/*
 * NULL JSFX - Reference: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullref {

static const char* const kListenOptions[] = { "TRACK", "REFERENCE" };
static const char* const kOffOnOptions[] = { "OFF", "ON" };
static const char* const kMonitorOptions[] = { "STEREO", "MONO", "SIDE" };

// slider1..slider4 of reference.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
    { "Level Match", "level_match", 0.0f, 1.0f, 1.0f, "", 1.0f, kOffOnOptions },
    { "Trim", "trim", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Monitor", "monitor", 0.0f, 2.0f, 0.0f, "", 1.0f, kMonitorOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullref
