/*
 * NULL JSFX - Amp Simulator: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullamp {

static const char* const kModelOptions[] = { "CLEAN", "CRUNCH", "LEAD", "BASS", "JAZZ", "BRITISH", "MODERN", "FUZZ" };
static const char* const kBrightOptions[] = { "OFF", "ON" };
static const char* const kOsOptions[] = { "2X", "4X" };

// slider1..slider12 of amp_simulator.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Model", "model", 0.0f, 7.0f, 1.0f, "", 1.0f, kModelOptions },
    { "Gain", "gain", 0.0f, 10.0f, 5.0f, "", 0.1f },
    { "Bass", "bass", 0.0f, 10.0f, 5.0f, "", 0.1f },
    { "Mid", "mid", 0.0f, 10.0f, 5.0f, "", 0.1f },
    { "Treble", "treble", 0.0f, 10.0f, 5.0f, "", 0.1f },
    { "Presence", "presence", 0.0f, 10.0f, 5.0f, "", 0.1f },
    { "Master", "master", 0.0f, 10.0f, 5.0f, "", 0.1f },
    { "Bright", "bright", 0.0f, 1.0f, 0.0f, "", 1.0f, kBrightOptions },
    { "Sag", "sag", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Oversampling", "oversampling", 0.0f, 1.0f, 1.0f, "", 1.0f, kOsOptions },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Noise Gate", "noise_gate", -90.0f, -20.0f, -90.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a 0.1-step value as the JSFX slider holds it (min + k * 0.1 in double)
inline double snap(float v, double min) { return min + std::round((double(v) - min) / 0.1) * 0.1; }

} // namespace nullamp
