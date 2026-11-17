/*
 * NULL JSFX - Choir: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullchoir {

static const char* const kSectionOptions[] = { "Auto", "Soprano", "Alto", "Tenor", "Bass" };
static const char* const kSectionShort[] = { "AUTO", "SOPRANO", "ALTO", "TENOR", "BASS" };
static const char* const kMpeOptions[] = { "Off", "On" };
static const char* const kMpeShort[] = { "OFF", "ON" };

// slider1..slider15 of choir.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Vowel", "vowel", 0.0f, 4.0f, 0.0f, "", 0.01f },
    { "Voices", "voices", 1.0f, 8.0f, 8.0f, "", 1.0f },
    { "Detune", "detune", 0.0f, 40.0f, 11.0f, "ct", 1.0f },
    { "Breath", "breath", 0.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Vibrato", "vibrato", 0.0f, 100.0f, 35.0f, "%", 1.0f },
    { "Vibrato Rate", "vibrato_rate", 3.0f, 8.0f, 5.4f, "Hz", 0.1f },
    { "Section", "section", 0.0f, 4.0f, 0.0f, "", 1.0f, kSectionOptions },
    { "Attack", "attack", 5.0f, 3000.0f, 250.0f, "ms", 1.0f },
    { "Release", "release", 20.0f, 5000.0f, 600.0f, "ms", 1.0f },
    { "Brightness", "brightness", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Stereo", "stereo", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "MPE", "mpe", 0.0f, 1.0f, 0.0f, "", 1.0f, kMpeOptions },
    { "MPE Bend Range", "mpe_bend_range", 0.0f, 96.0f, 48.0f, "st", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullchoir
