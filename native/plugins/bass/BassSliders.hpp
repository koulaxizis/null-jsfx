/*
 * NULL JSFX - Bass: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullbass {

static const char* const kOscOptions[] = { "Sub", "Saw", "Square", "808" };
static const char* const kOscShort[] = { "SUB", "SAW", "SQUARE", "808" };
static const char* const kGlideOptions[] = { "Legato", "Always" };
static const char* const kGlideShort[] = { "LEGATO", "ALWAYS" };
static const char* const kPhraseOptions[] = { "Off", "Eighths", "Octaves", "Offbeat", "Gallop", "Funk", "Acid", "Tresillo", "Trap" };
static const char* const kPhraseShort[] = { "OFF", "EIGHTHS", "OCTAVES", "OFFBEAT", "GALLOP", "FUNK", "ACID", "TRESILLO", "TRAP" };

// slider1..slider19 of bass.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Oscillator", "oscillator", 0.0f, 3.0f, 1.0f, "", 1.0f, kOscOptions },
    { "Octave", "octave", -2.0f, 2.0f, 0.0f, "", 1.0f },
    { "Detune", "detune", 0.0f, 50.0f, 0.0f, "cents", 1.0f },
    { "Sub Level", "sub_level", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Punch", "punch", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Drive", "drive", 0.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Cutoff", "cutoff", 30.0f, 12000.0f, 900.0f, "Hz", 1.0f },
    { "Resonance", "resonance", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Env Amount", "env_amount", -100.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Filter Decay", "filter_decay", 5.0f, 3000.0f, 250.0f, "ms", 1.0f },
    { "Attack", "attack", 1.0f, 2000.0f, 2.0f, "ms", 1.0f },
    { "Decay", "decay", 5.0f, 5000.0f, 400.0f, "ms", 1.0f },
    { "Sustain", "sustain", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Release", "release", 5.0f, 5000.0f, 80.0f, "ms", 1.0f },
    { "Glide Mode", "glide_mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kGlideOptions },
    { "Glide", "glide", 0.0f, 1000.0f, 30.0f, "ms", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Phrase", "phrase", 0.0f, 8.0f, 0.0f, "", 1.0f, kPhraseOptions },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullbass
