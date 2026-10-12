/*
 * NULL JSFX - De-Hum: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <cmath>

namespace nulldehum {

static const char* const kMainsOptions[] = { "50 HZ", "60 HZ" };
static const char* const kLowCutOptions[] = { "OFF", "20 HZ", "30 HZ", "40 HZ" };

// slider1..slider6 of de_hum.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Frequency", "frequency", 0.0f, 1.0f, 0.0f, "", 1.0f, kMainsOptions },
    { "Harmonics", "harmonics", 1.0f, 8.0f, 4.0f, "", 1.0f },
    { "Q", "q", 5.0f, 100.0f, 30.0f, "", 1.0f },
    { "Depth", "depth", 0.0f, 60.0f, 40.0f, "dB", 1.0f },
    { "Fine", "fine", -2.0f, 2.0f, 0.0f, "Hz", 0.01f },
    { "Low Cut", "low_cut", 0.0f, 3.0f, 0.0f, "", 1.0f, kLowCutOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 1.0, 0.0, 1.0 },
    { 1.0, 8.0, 4.0, 1.0 },
    { 5.0, 100.0, 30.0, 1.0 },
    { 0.0, 60.0, 40.0, 1.0 },
    { -2.0, 2.0, 0.0, 0.01 },
    { 0.0, 3.0, 0.0, 1.0 },
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

} // namespace nulldehum
