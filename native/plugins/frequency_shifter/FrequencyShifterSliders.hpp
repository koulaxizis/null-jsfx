/*
 * NULL JSFX - Frequency Shifter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <cmath>

namespace nullfreqshift {


// slider1..slider4 of frequency_shifter.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Shift", "shift", -2000.0f, 2000.0f, 100.0f, "Hz", 0.1f },
    { "Stereo Offset", "offset", -100.0f, 100.0f, 0.0f, "Hz", 0.1f },
    { "Feedback", "feedback", 0.0f, 95.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { -2000.0, 2000.0, 100.0, 0.1 },
    { -100.0, 100.0, 0.0, 0.1 },
    { 0.0, 95.0, 0.0, 1.0 },
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

} // namespace nullfreqshift
