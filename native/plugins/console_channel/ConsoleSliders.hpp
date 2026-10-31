/*
 * NULL JSFX - Console Channel and Console Buss: the JSFX sliders (the same three in both),
 * shared by the plugins, their interfaces and the Rack modules
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullconsole {

static const char* const kCharacterOptions[] = { "Clean", "Warm", "Punchy", "Bright" };

// slider1..slider3 of console_channel.jsfx and console_buss.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Character", "character", 0.0f, 3.0f, 1.0f, "", 1.0f, kCharacterOptions },
    { "Drive", "drive", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the interface's labels for the selector
static const char* const kUiCharacter[] = { "CLEAN", "WARM", "PUNCHY", "BRIGHT" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Character", "character", 0.0f, 3.0f, 1.0f, "", 1.0f, kUiCharacter },
    kSliders[1], kSliders[2],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullconsole
