/*
 * NULL JSFX - Assisted EQ: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullassistedeq {

static const char* const kLearnOptions[] = { "Off", "On" };
static const char* const kSourceOptions[] = { "Vocal", "Drums", "Bass", "Instrument", "Mix" };
static const char* const kTargetOptions[] = { "Natural", "Warm", "Bright", "Punchy", "Radio", "Podcast", "Lo-Fi" };
static const char* const kLearnedOptions[] = { "No", "Yes" };

// slider1..slider16 of assisted_eq.jsfx, in order; 9-16 are hidden in the JSFX and hold what
// Learn measured
static const DISTRHO::SliderInfo kSliders[] = {
    { "Learn", "learn", 0.0f, 1.0f, 0.0f, "", 1.0f, kLearnOptions },
    { "Source", "source", 0.0f, 4.0f, 0.0f, "", 1.0f, kSourceOptions },
    { "Target", "target", 0.0f, 6.0f, 0.0f, "", 1.0f, kTargetOptions },
    { "Amount", "amount", 0.0f, 150.0f, 100.0f, "%", 1.0f },
    { "Low Trim", "low_trim", -6.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Mid Trim", "mid_trim", -6.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "High Trim", "high_trim", -6.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Learned", "learned", 0.0f, 1.0f, 0.0f, "", 1.0f, kLearnedOptions },
    { "Learned Low", "learned_low", -24.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Learned Mud", "learned_mud", -24.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Learned Mud Freq", "learned_mud_freq", 200.0f, 500.0f, 300.0f, "Hz", 1.0f },
    { "Learned Honk", "learned_honk", -24.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Learned Presence", "learned_presence", -24.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Learned Air", "learned_air", -24.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Learned Lowest", "learned_lowest", 20.0f, 500.0f, 20.0f, "Hz", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullassistedeq
