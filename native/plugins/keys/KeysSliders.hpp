/*
 * NULL JSFX - Keys: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullkeys {

static const char* const kModelOptions[] = { "Tine EP", "Reed EP", "FM EP", "Music Box", "Celesta", "Glockenspiel", "Vibraphone", "Marimba", "Kalimba" };
static const char* const kModelShort[] = { "TINE EP", "REED EP", "FM EP", "MUSIC BOX", "CELESTA", "GLOCKEN", "VIBES", "MARIMBA", "KALIMBA" };
static const char* const kTremOptions[] = { "Mono", "Stereo" };
static const char* const kTremShort[] = { "MONO", "STEREO" };
static const char* const kMpeOptions[] = { "Off", "On" };
static const char* const kMpeShort[] = { "OFF", "ON" };

// slider1..slider12 of keys.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Model", "model", 0.0f, 8.0f, 0.0f, "", 1.0f, kModelOptions },
    { "Tone", "tone", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Bell", "bell", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 10.0f, 400.0f, 100.0f, "%", 1.0f },
    { "Release", "release", 10.0f, 3000.0f, 250.0f, "ms", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Tremolo Rate", "tremolo_rate", 0.5f, 12.0f, 4.5f, "Hz", 0.1f },
    { "Tremolo Depth", "tremolo_depth", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Tremolo Mode", "tremolo_mode", 0.0f, 1.0f, 1.0f, "", 1.0f, kTremOptions },
    { "Drive", "drive", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Chorus", "chorus", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "MPE", "mpe", 0.0f, 1.0f, 0.0f, "", 1.0f, kMpeOptions },
    { "MPE Bend Range", "mpe_bend_range", 0.0f, 96.0f, 48.0f, "st", 1.0f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullkeys
