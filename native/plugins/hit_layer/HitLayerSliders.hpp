/*
 * NULL JSFX - Hit Layer: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullhitlayer {

// slider1..slider12 of hit_layer.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", -60.0f, 0.0f, -24.0f, "dB", 0.1f },
    { "Tone", "tone", 30.0f, 2000.0f, 60.0f, "Hz", 1.0f },
    { "Tone Level", "tone_level", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Tone Decay", "tone_decay", 10.0f, 2000.0f, 200.0f, "ms", 1.0f },
    { "Pitch Drop", "pitch_drop", 0.0f, 24.0f, 12.0f, "st", 0.1f },
    { "Noise Level", "noise_level", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Noise Decay", "noise_decay", 5.0f, 1000.0f, 80.0f, "ms", 1.0f },
    { "Noise Color", "noise_color", 200.0f, 16000.0f, 4000.0f, "Hz", 1.0f },
    { "Follow", "follow", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Width", "width", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Dry", "dry", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullhitlayer
