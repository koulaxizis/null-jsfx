/*
 * NULL JSFX - Harmonizer: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullharmonizer {

static const char* const kModeOptions[] = { "SEMI", "SCALE", "MIDI" };
static const char* const kKeyOptions[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kScaleOptions[] = { "MAJOR", "MINOR", "DORIAN", "MIXOLYD", "HARM MIN", "PENT MAJ", "PENT MIN" };
static const char* const kOffOn[] = { "OFF", "ON" };

// slider1..slider12 of harmonizer.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Voice 1 Interval", "interval1", -12.0f, 12.0f, 4.0f, "st", 1.0f },
    { "Voice 2 Interval", "interval2", -12.0f, 12.0f, 7.0f, "st", 1.0f },
    { "Voice 1 Level", "level1", -48.0f, 0.0f, -6.0f, "dB", 0.5f },
    { "Voice 2 Level", "level2", -48.0f, 0.0f, -6.0f, "dB", 0.5f },
    { "Detune", "detune", 0.0f, 30.0f, 8.0f, "ct", 0.5f },
    { "Delay", "delay", 0.0f, 80.0f, 15.0f, "ms", 1.0f },
    { "Spread", "spread", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mode", "mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Key", "key", 0.0f, 11.0f, 0.0f, "", 1.0f, kKeyOptions },
    { "Scale", "scale", 0.0f, 6.0f, 0.0f, "", 1.0f, kScaleOptions },
    { "Formant Preserve", "formant", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullharmonizer
