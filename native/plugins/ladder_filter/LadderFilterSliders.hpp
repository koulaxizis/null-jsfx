/*
 * NULL JSFX - Ladder Filter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullladder {

static const char* const kModeOptions[] = { "LP", "HP" };

// slider1..slider5 of ladder_filter.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Cutoff", "cutoff", 20.0f, 20000.0f, 1000.0f, "Hz", 1.0f },
    { "Resonance", "resonance", 0.0f, 4.0f, 0.5f, "", 0.1f },
    { "Drive", "drive", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Gain", "gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 20.0, 20000.0, 1000.0, 1.0 },
    { 0.0, 4.0, 0.5, 0.1 },
    { 0.0, 100.0, 0.0, 1.0 },
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

} // namespace nullladder
