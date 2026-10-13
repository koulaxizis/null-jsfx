/*
 * NULL JSFX - Sub Bloom: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullsubbloom {

static const char* const kOctaveOptions[] = { "-1", "-2", "BOTH" };

// slider1..slider5 of sub_bloom.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Source Frequency", "source", 50.0f, 300.0f, 100.0f, "Hz", 1.0f },
    { "Sub Level", "sub", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Sub Octave", "octave", 0.0f, 2.0f, 0.0f, "", 1.0f, kOctaveOptions },
    { "Original Mix", "original", 0.0f, 100.0f, 80.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 50.0, 300.0, 100.0, 1.0 },
    { 0.0, 100.0, 50.0, 1.0 },
    { 0.0, 2.0, 0.0, 1.0 },
    { 0.0, 100.0, 80.0, 1.0 },
    { -12.0, 12.0, 0.0, 1.0 },
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

} // namespace nullsubbloom
