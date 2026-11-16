/*
 * NULL JSFX - Phase Distortion parameters, shared by Plugin.cpp, UI.cpp and the Rack module
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullphasedist {

static const char* const kSourceOptions[] = { "SELF", "SIDECHAIN" };

// slider1..slider7 of phase_distortion.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Drive", "drive", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Tone", "tone", 100.0f, 20000.0f, 2000.0f, "Hz", 1.0f },
    { "Bias", "bias", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Spread", "spread", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Source", "source", 0.0f, 1.0f, 0.0f, "", 1.0f, kSourceOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullphasedist
