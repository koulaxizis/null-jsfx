/*
 * NULL JSFX - Formant Filter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullformant {

static const char* const kVowelOptions[] = { "A", "E", "I", "O", "U" };

// slider1..slider3 of formant_filter.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Vowel", "vowel", 0.0f, 4.0f, 0.0f, "", 1.0f, kVowelOptions },
    { "Gain", "gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Q", "q", 5.0f, 20.0f, 10.0f, "", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 4.0, 0.0, 1.0 },
    { -12.0, 12.0, 0.0, 0.1 },
    { 5.0, 20.0, 10.0, 1.0 },
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

} // namespace nullformant
