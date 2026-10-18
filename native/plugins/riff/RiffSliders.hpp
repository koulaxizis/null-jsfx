/*
 * NULL JSFX - Riff: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullriff {

static const char* const kKeyOptions[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kScaleOptions[] = { "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian",
                                             "Lydian", "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic",
                                             "Blues" };
static const char* const kRateOptions[] = { "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/32T" };

// slider1..slider12 of riff.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Key", "key", 0.0f, 11.0f, 0.0f, "", 1.0f, kKeyOptions },
    { "Scale", "scale", 0.0f, 11.0f, 0.0f, "", 1.0f, kScaleOptions },
    { "Seed", "seed", 1.0f, 999.0f, 1.0f, "", 1.0f },
    { "Density", "density", 0.0f, 100.0f, 75.0f, "%", 1.0f },
    { "Steps", "steps", 4.0f, 32.0f, 16.0f, "", 1.0f },
    { "Rate", "rate", 0.0f, 5.0f, 1.0f, "", 1.0f, kRateOptions },
    { "Octave Range", "octaves", 1.0f, 3.0f, 1.0f, "", 1.0f },
    { "Note Length", "length", 10.0f, 100.0f, 80.0f, "%", 1.0f },
    { "Rhythm Lock", "lock", 0.0f, 999.0f, 0.0f, "", 1.0f },
    { "Velocity Range", "velocity", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Accent", "accent", 0.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Variation", "variation", 0.0f, 100.0f, 50.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the selectors' short upper-case names, for the interface (as riff_gfx.jsfx)
static const char* const kScaleShort[] = { "MAJOR", "MINOR", "HARM", "MELODIC", "DORIAN", "PHRYG", "LYDIAN", "MIXO",
                                           "LOCRIAN", "MAJ PENT", "MIN PENT", "BLUES" };

static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Scale", "scale", 0.0f, 11.0f, 0.0f, "", 1.0f, kScaleShort },
    kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6], kSliders[7], kSliders[8], kSliders[9],
    kSliders[10], kSliders[11],
};

} // namespace nullriff
