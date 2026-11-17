/*
 * NULL JSFX - Wind: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullwind {

static const char* const kModelOptions[] = { "Flute", "Clarinet", "Saxophone", "Brass" };
static const char* const kModelShort[] = { "FLUTE", "CLARINET", "SAX", "BRASS" };
static const char* const kSourceOptions[] = { "Velocity", "Mod Wheel", "Breath CC 2", "Aftertouch", "Expression CC 11" };
static const char* const kSourceShort[] = { "VELOCITY", "MOD WHEEL", "BREATH CC", "AFTERTOUCH", "EXPRESSION" };

// slider1..slider12 of wind.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Model", "model", 0.0f, 3.0f, 0.0f, "", 1.0f, kModelOptions },
    { "Breath Source", "breath_source", 0.0f, 4.0f, 0.0f, "", 1.0f, kSourceOptions },
    { "Breath", "breath", 0.0f, 100.0f, 90.0f, "%", 1.0f },
    { "Noise", "noise", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Tone", "tone", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Vibrato", "vibrato", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Vibrato Rate", "vibrato_rate", 3.0f, 8.0f, 5.3f, "Hz", 0.1f },
    { "Vibrato Delay", "vibrato_delay", 0.0f, 2000.0f, 250.0f, "ms", 1.0f },
    { "Attack", "attack", 1.0f, 1000.0f, 30.0f, "ms", 1.0f },
    { "Release", "release", 5.0f, 2000.0f, 100.0f, "ms", 1.0f },
    { "Glide", "glide", 0.0f, 500.0f, 40.0f, "ms", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullwind
