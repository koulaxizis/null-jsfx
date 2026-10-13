/*
 * NULL JSFX - Envelope Follower: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullenvfollower {

static const char* const kTargetOptions[] = { "LPF", "BPF", "HPF", "VOLUME" };
static const char* const kDirectionOptions[] = { "UP", "DOWN" };

// slider1..slider9 of envelope_follower.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Target", "target", 0.0f, 3.0f, 0.0f, "", 1.0f, kTargetOptions },
    { "Attack", "attack", 0.1f, 100.0f, 5.0f, "ms", 0.1f },
    { "Release", "release", 10.0f, 2000.0f, 150.0f, "ms", 1.0f },
    { "Sensitivity", "sensitivity", -24.0f, 24.0f, 0.0f, "dB", 0.5f },
    { "Low Freq", "low_freq", 20.0f, 2000.0f, 200.0f, "Hz", 1.0f },
    { "High Freq", "high_freq", 500.0f, 15000.0f, 3000.0f, "Hz", 10.0f },
    { "Resonance", "resonance", 0.5f, 10.0f, 3.0f, "", 0.1f },
    { "Direction", "direction", 0.0f, 1.0f, 0.0f, "", 1.0f, kDirectionOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullenvfollower
