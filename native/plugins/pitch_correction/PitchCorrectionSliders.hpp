/*
 * NULL JSFX - Pitch Correction: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullpitchcorrection {

static const char* const kKeyOptions[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kScaleOptions[] = { "CHROM", "MAJOR", "MINOR", "HARM MIN", "PENTA MAJ",
                                             "PENTA MIN", "BLUES", "DORIAN", "MIXOLYD" };
static const char* const kTargetOptions[] = { "SCALE", "MIDI" };

// slider1..slider10 of pitch_correction.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Key", "key", 0.0f, 11.0f, 0.0f, "", 1.0f, kKeyOptions },
    { "Scale", "scale", 0.0f, 8.0f, 0.0f, "", 1.0f, kScaleOptions },
    { "Retune Speed", "retune", 0.0f, 400.0f, 20.0f, "ms", 1.0f },
    { "Humanize", "humanize", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Transpose", "transpose", -12.0f, 12.0f, 0.0f, "st", 1.0f },
    { "Sensitivity", "sensitivity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Target", "target", 0.0f, 1.0f, 0.0f, "", 1.0f, kTargetOptions },
    { "Vibrato Rate", "vibrato_rate", 0.0f, 10.0f, 5.0f, "Hz", 0.1f },
    { "Vibrato Depth", "vibrato_depth", 0.0f, 100.0f, 0.0f, "ct", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// pitch classes of each Scale above the Key, bit n = n semitones (the JSFX's smask)
static constexpr int kScaleMasks[] = { 4095, 2741, 1453, 2477, 661, 1193, 1257, 1709, 1717 };

} // namespace nullpitchcorrection
