/*
 * NULL JSFX - OTT: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullott {

// slider1..slider8 of ott.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Depth", "depth", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Time", "time", 0.0f, 400.0f, 100.0f, "%", 1.0f },
    { "Upward", "upward", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Downward", "downward", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Low Gain", "low_gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mid Gain", "mid_gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "High Gain", "high_gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullott
