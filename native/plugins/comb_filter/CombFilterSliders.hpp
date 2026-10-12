/*
 * NULL JSFX - Comb Filter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <cmath>

namespace nullcomb {

static const char* const kModeOptions[] = { "FWD", "BACK" };

// slider1..slider4 of comb_filter.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Frequency", "frequency", 20.0f, 20000.0f, 500.0f, "Hz", 1.0f },
    { "Feedback", "feedback", 0.0f, 0.99f, 0.5f, "", 0.01f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Gain", "gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 20.0, 20000.0, 500.0, 1.0 },
    { 0.0, 0.99, 0.5, 0.01 },
    { 0.0, 1.0, 0.0, 1.0 },
    { -12.0, 12.0, 0.0, 0.1 },
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

} // namespace nullcomb
