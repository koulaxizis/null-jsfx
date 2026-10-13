/*
 * NULL JSFX - Multiband Comp: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullmultiband {

static const char* const kModeOptions[] = { "ACTIVE", "BYPASS", "MUTE", "SOLO" };

// slider1..slider23 of multiband_compressor.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Crossover 1", "crossover1", 50.0f, 1000.0f, 120.0f, "Hz", 1.0f },
    { "Crossover 2", "crossover2", 500.0f, 4000.0f, 1000.0f, "Hz", 1.0f },
    { "Crossover 3", "crossover3", 4000.0f, 12000.0f, 6000.0f, "Hz", 1.0f },
    { "Low Threshold", "low_threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
    { "Low Ratio", "low_ratio", 1.0f, 20.0f, 3.0f, "", 0.1f },
    { "Low Makeup", "low_makeup", 0.0f, 12.0f, 0.0f, "dB", 1.0f },
    { "Low Mode", "low_mode", 0.0f, 3.0f, 0.0f, "", 1.0f, kModeOptions },
    { "LowMid Threshold", "lowmid_threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
    { "LowMid Ratio", "lowmid_ratio", 1.0f, 20.0f, 3.0f, "", 0.1f },
    { "LowMid Makeup", "lowmid_makeup", 0.0f, 12.0f, 0.0f, "dB", 1.0f },
    { "LowMid Mode", "lowmid_mode", 0.0f, 3.0f, 0.0f, "", 1.0f, kModeOptions },
    { "HighMid Threshold", "highmid_threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
    { "HighMid Ratio", "highmid_ratio", 1.0f, 20.0f, 3.0f, "", 0.1f },
    { "HighMid Makeup", "highmid_makeup", 0.0f, 12.0f, 0.0f, "dB", 1.0f },
    { "HighMid Mode", "highmid_mode", 0.0f, 3.0f, 0.0f, "", 1.0f, kModeOptions },
    { "High Threshold", "high_threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
    { "High Ratio", "high_ratio", 1.0f, 20.0f, 3.0f, "", 0.1f },
    { "High Makeup", "high_makeup", 0.0f, 12.0f, 0.0f, "dB", 1.0f },
    { "High Mode", "high_mode", 0.0f, 3.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Attack", "attack", 0.0f, 200.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 0.0f, 1000.0f, 100.0f, "ms", 1.0f },
    { "Knee", "knee", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Master Output", "master_output", 0.0f, 12.0f, 0.0f, "dB", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a 0.1-step slider value as the JSFX sees it (the parameter arrives as a float)
inline double tenth(float v) noexcept { return std::round(double(v) * 10.0) / 10.0 + 0.0; }

} // namespace nullmultiband
