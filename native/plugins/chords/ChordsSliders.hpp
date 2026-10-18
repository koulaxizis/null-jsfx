/*
 * NULL JSFX - Chords: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullchords {

static const char* const kModeOptions[] = { "Chord", "Progression" };
static const char* const kKeyOptions[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kScaleOptions[] = { "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian",
                                             "Phrygian", "Lydian", "Mixolydian", "Locrian" };
static const char* const kTypeOptions[] = { "Triad", "7th", "9th", "Sus2", "Sus4", "Power" };
static const char* const kInvOptions[] = { "Root", "1st", "2nd", "3rd" };
static const char* const kVoicingOptions[] = { "Close", "Drop 2", "Open", "Wide" };
static const char* const kDirOptions[] = { "Up", "Down", "Alternate" };
static const char* const kProgOptions[] = { "I-V-vi-IV", "I-vi-IV-V", "vi-IV-I-V", "I-IV-vi-V", "ii-V-I", "Canon",
                                            "i-VI-III-VII", "i-VII-VI-V", "i-iv-V-i" };

// slider1..slider11 of chords.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Key", "key", 0.0f, 11.0f, 0.0f, "", 1.0f, kKeyOptions },
    { "Scale", "scale", 0.0f, 8.0f, 0.0f, "", 1.0f, kScaleOptions },
    { "Chord Type", "type", 0.0f, 5.0f, 0.0f, "", 1.0f, kTypeOptions },
    { "Inversion", "inversion", 0.0f, 3.0f, 0.0f, "", 1.0f, kInvOptions },
    { "Voicing", "voicing", 0.0f, 3.0f, 0.0f, "", 1.0f, kVoicingOptions },
    { "Octave", "octave", -2.0f, 2.0f, 0.0f, "", 1.0f },
    { "Strum", "strum", 0.0f, 200.0f, 0.0f, "ms", 1.0f },
    { "Strum Direction", "direction", 0.0f, 2.0f, 0.0f, "", 1.0f, kDirOptions },
    { "Velocity Curve", "curve", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Progression", "progression", 0.0f, 8.0f, 0.0f, "", 1.0f, kProgOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the selectors' short upper-case names, for the interface (as chords_gfx.jsfx)
static const char* const kModeShort[] = { "CHORD", "PROG" };
static const char* const kScaleShort[] = { "MAJOR", "MINOR", "HARM", "MELODIC", "DORIAN", "PHRYG", "LYDIAN", "MIXO", "LOCRIAN" };
static const char* const kTypeShort[] = { "TRIAD", "7TH", "9TH", "SUS2", "SUS4", "POWER" };
static const char* const kInvShort[] = { "ROOT", "1ST", "2ND", "3RD" };
static const char* const kVoicingShort[] = { "CLOSE", "DROP 2", "OPEN", "WIDE" };
static const char* const kDirShort[] = { "UP", "DOWN", "ALT" };
// the progressions by a short name (the display shows their steps)
static const char* const kProgShort[] = { "POP", "DOO-WOP", "SAD POP", "POP 2", "JAZZ", "CANON", "EPIC", "SPANISH", "MINOR" };

static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeShort },
    kSliders[1],
    { "Scale", "scale", 0.0f, 8.0f, 0.0f, "", 1.0f, kScaleShort },
    { "Chord Type", "type", 0.0f, 5.0f, 0.0f, "", 1.0f, kTypeShort },
    { "Inversion", "inversion", 0.0f, 3.0f, 0.0f, "", 1.0f, kInvShort },
    { "Voicing", "voicing", 0.0f, 3.0f, 0.0f, "", 1.0f, kVoicingShort },
    kSliders[6], kSliders[7],
    { "Strum Direction", "direction", 0.0f, 2.0f, 0.0f, "", 1.0f, kDirShort },
    kSliders[9],
    { "Progression", "progression", 0.0f, 8.0f, 0.0f, "", 1.0f, kProgShort },
};

} // namespace nullchords
