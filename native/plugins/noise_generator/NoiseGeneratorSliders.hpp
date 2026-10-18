/*
 * NULL JSFX - Noise Generator: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullnoise {

static const char* const kTypeOptions[] = { "WHITE", "PINK", "BROWN" };
static const char* const kMixOptions[] = { "Replace", "Add" };
static const char* const kMixShort[] = { "REPL", "ADD" };

// slider1..slider8 of noise_generator.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Type", "type", 0.0f, 2.0f, 0.0f, "", 1.0f, kTypeOptions },
    { "Level", "level", -60.0f, 0.0f, -24.0f, "dB", 0.1f },
    { "Stereo Width", "width", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Low Cut", "low_cut", 20.0f, 2000.0f, 20.0f, "Hz", 1.0f },
    { "High Cut", "high_cut", 500.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Follow Input", "follow", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Ducking", "ducking", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix Mode", "mix_mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kMixOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's short mix mode names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6],
    { "Mix Mode", "mix_mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kMixShort },
};

} // namespace nullnoise
