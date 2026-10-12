/*
 * NULL JSFX - Resonance Suppressor: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullrsp {

static const char* const kModeOptions[] = { "SOFT", "HARD" };
static const char* const kDeltaOptions[] = { "OFF", "ON" };

// slider1..slider10 of resonance_suppressor.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Depth", "depth", 0.0f, 24.0f, 6.0f, "dB", 0.1f },
    { "Sharpness", "sharpness", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Selectivity", "selectivity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Attack", "attack", 1.0f, 200.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 10.0f, 1000.0f, 120.0f, "ms", 1.0f },
    { "Low Freq", "low_freq", 20.0f, 2000.0f, 100.0f, "Hz", 1.0f },
    { "High Freq", "high_freq", 1000.0f, 20000.0f, 18000.0f, "Hz", 1.0f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Delta", "delta", 0.0f, 1.0f, 0.0f, "", 1.0f, kDeltaOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullrsp
