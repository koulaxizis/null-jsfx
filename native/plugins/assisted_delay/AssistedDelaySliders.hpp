/*
 * NULL JSFX - Assisted Delay: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullassisteddelay {

static const char* const kLearnOptions[] = { "Off", "On" };
static const char* const kSourceOptions[] = { "Vocal", "Drums", "Bass", "Instrument", "Mix" };
static const char* const kTargetOptions[] = { "Natural", "Warm", "Bright", "Punchy", "Radio", "Podcast", "Lo-Fi" };
static const char* const kLearnedOptions[] = { "No", "Yes" };

// slider1..slider13 of assisted_delay.jsfx, in order; 9-13 are hidden in the JSFX and hold what
// Learn measured
static const DISTRHO::SliderInfo kSliders[] = {
    { "Learn", "learn", 0.0f, 1.0f, 0.0f, "", 1.0f, kLearnOptions },
    { "Source", "source", 0.0f, 4.0f, 0.0f, "", 1.0f, kSourceOptions },
    { "Target", "target", 0.0f, 6.0f, 0.0f, "", 1.0f, kTargetOptions },
    { "Amount", "amount", 0.0f, 150.0f, 100.0f, "%", 1.0f },
    { "Repeats Trim", "repeats_trim", -2.0f, 2.0f, 0.0f, "", 0.1f },
    { "Tone Trim", "tone_trim", -2.0f, 2.0f, 0.0f, "", 0.1f },
    { "Level Trim", "level_trim", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Learned", "learned", 0.0f, 1.0f, 0.0f, "", 1.0f, kLearnedOptions },
    { "Learned Loud", "learned_loud", -80.0f, 0.0f, -18.0f, "dB", 0.1f },
    { "Learned Median", "learned_median", -80.0f, 0.0f, -24.0f, "dB", 0.1f },
    { "Learned Onset", "learned_onset", 0.0f, 2000.0f, 0.0f, "ms", 1.0f },
    { "Learned Bright", "learned_bright", -60.0f, 0.0f, -18.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullassisteddelay
