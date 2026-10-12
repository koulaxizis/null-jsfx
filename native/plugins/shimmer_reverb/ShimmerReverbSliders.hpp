/*
 * NULL JSFX - Shimmer Reverb: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullshimmer {

static const char* const kPitchOptions[] = { "+12 (Octave)", "+7 (Fifth)", "+19 (Octave + Fifth)", "+24 (Two Octaves)", "-12 (Octave Down)" };

// slider1..slider6 of shimmer_reverb.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Size", "size", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Decay", "decay", 1.0f, 20.0f, 6.0f, "s", 0.1f },
    { "Shimmer", "shimmer", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Pitch", "pitch", 0.0f, 4.0f, 0.0f, "", 1.0f, kPitchOptions },
    { "Damping", "damping", 1000.0f, 16000.0f, 6000.0f, "Hz", 10.0f },
    { "Mix", "mix", 0.0f, 100.0f, 35.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullshimmer
