/*
 * NULL JSFX - Transient Shaper: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulltransient {

static const char* const kSpeed[] = { "FAST", "MEDIUM", "SLOW" };
static const char* const kOffOn[] = { "OFF", "ON" };

// slider1..slider8 of transient_shaper.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Attack", "attack", -20.0f, 20.0f, 0.0f, "dB", 0.1f },
    { "Sustain", "sustain", -20.0f, 20.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Speed", "speed", 0.0f, 2.0f, 1.0f, "", 1.0f, kSpeed },
    { "Focus Low", "focus_low", 20.0f, 20000.0f, 20.0f, "Hz", 1.0f },
    { "Focus High", "focus_high", 20.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Output Clip", "output_clip", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulltransient
