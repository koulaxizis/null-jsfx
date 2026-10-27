/*
 * NULL JSFX - Spectral Ducker: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullsduck {

static const char* const kChannelsOptions[] = { "STEREO", "MID", "SIDE" };
static const char* const kListenOptions[] = { "OUTPUT", "DELTA" };

// slider1..slider10 of spectral_ducker.jsfx, in order (Attack, Release, Low Cut and High Cut
// are log-shaped in the JSFX)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Amount", "amount", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Threshold", "threshold", -60.0f, 0.0f, -40.0f, "dB", 0.5f },
    { "Attack", "attack", 0.1f, 100.0f, 5.0f, "ms", 0.1f },
    { "Release", "release", 10.0f, 1000.0f, 150.0f, "ms", 1.0f },
    { "Low Cut", "low_cut", 20.0f, 5000.0f, 20.0f, "Hz", 1.0f },
    { "High Cut", "high_cut", 200.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Channels", "channels", 0.0f, 2.0f, 0.0f, "", 1.0f, kChannelsOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullsduck
