/*
 * NULL JSFX - Resonator: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <cmath>

namespace nullres {

static const char* const kRootOptions[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kChordOptions[] = { "UNISON", "OCTAVES", "FIFTH", "MAJOR", "MINOR", "SUS4", "MAJ7", "MIN7" };

// slider1..slider7 of resonator.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Root Note", "root", 0.0f, 11.0f, 0.0f, "", 1.0f, kRootOptions },
    { "Octave", "octave", 1.0f, 6.0f, 3.0f, "", 1.0f },
    { "Chord", "chord", 0.0f, 7.0f, 3.0f, "", 1.0f, kChordOptions },
    { "Decay", "decay", 0.1f, 10.0f, 1.5f, "s", 0.01f },
    { "Brightness", "brightness", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Spread", "spread", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 11.0, 0.0, 1.0 },
    { 1.0, 6.0, 3.0, 1.0 },
    { 0.0, 7.0, 3.0, 1.0 },
    { 0.1, 10.0, 1.5, 0.01 },
    { 0.0, 100.0, 50.0, 1.0 },
    { 0.0, 100.0, 30.0, 1.0 },
    { 0.0, 100.0, 50.0, 1.0 },
};

// The host passes parameters as float, the JSFX keeps the slider in double. Rebuild that double:
// the exact min / max / default, else the nearest point of the slider's step grid.
inline double sliderValue(uint32_t i, float v) noexcept
{
    const Range& r = kRanges[i];
    if (v == float(r.min)) return r.min;
    if (v == float(r.max)) return r.max;
    if (v == float(r.def)) return r.def;
    return std::fmax(r.min, std::fmin(r.max, r.min + std::round((double(v) - r.min) / r.step) * r.step));
}

} // namespace nullres
