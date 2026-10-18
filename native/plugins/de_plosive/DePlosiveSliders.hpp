/*
 * NULL JSFX - De-Plosive: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nulldeplosive {

static const char* const kListenOptions[] = { "OFF", "ON" };

// slider1..slider5 of de_plosive.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Sensitivity", "sensitivity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Frequency", "frequency", 60.0f, 400.0f, 150.0f, "Hz", 1.0f },
    { "Max Reduction", "max_reduction", 0.0f, 30.0f, 18.0f, "dB", 1.0f },
    { "Release", "release", 20.0f, 300.0f, 80.0f, "ms", 1.0f },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 100.0, 50.0, 1.0 },
    { 60.0, 400.0, 150.0, 1.0 },
    { 0.0, 30.0, 18.0, 1.0 },
    { 20.0, 300.0, 80.0, 1.0 },
    { 0.0, 1.0, 0.0, 1.0 },
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

} // namespace nulldeplosive
