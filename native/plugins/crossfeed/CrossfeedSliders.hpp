/*
 * NULL JSFX - Crossfeed: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullxfeed {

static const char* const kModeOptions[] = { "BS2B", "SPEAKERS" };

// slider1..slider7 of crossfeed.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Cutoff", "cutoff", 300.0f, 2000.0f, 700.0f, "Hz", 10.0f },
    { "Feed", "feed", 1.0f, 15.0f, 4.5f, "dB", 0.5f },
    { "Angle", "angle", 10.0f, 60.0f, 30.0f, "deg", 1.0f },
    { "Room", "room", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullxfeed
