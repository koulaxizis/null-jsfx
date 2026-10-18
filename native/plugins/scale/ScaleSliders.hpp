/*
 * NULL JSFX - Scale: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullscale {

static const char* const kKeyOptions[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kScaleOptions[] = { "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian",
                                             "Lydian", "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic", "Blues" };
static const char* const kModeOptions[] = { "Nearest", "Up", "Down", "White Keys", "Filter" };

// slider1..slider5 of scale.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Key", "key", 0.0f, 11.0f, 0.0f, "", 1.0f, kKeyOptions },
    { "Scale", "scale", 0.0f, 11.0f, 0.0f, "", 1.0f, kScaleOptions },
    { "Mode", "mode", 0.0f, 4.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Transpose", "transpose", -12.0f, 12.0f, 0.0f, "st", 1.0f },
    { "Octave", "octave", -3.0f, 3.0f, 0.0f, "", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the selectors' short upper-case names, for the interface (as scale_gfx.jsfx)
static const char* const kScaleShort[] = { "MAJOR", "MINOR", "HARM", "MELODIC", "DORIAN", "PHRYG", "LYDIAN", "MIXO",
                                           "LOCRIAN", "MAJ PEN", "MIN PEN", "BLUES" };
static const char* const kModeShort[] = { "NEAR", "UP", "DOWN", "WHITE", "FILTER" };

static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Scale", "scale", 0.0f, 11.0f, 0.0f, "", 1.0f, kScaleShort },
    { "Mode", "mode", 0.0f, 4.0f, 0.0f, "", 1.0f, kModeShort },
    kSliders[3], kSliders[4],
};

} // namespace nullscale
