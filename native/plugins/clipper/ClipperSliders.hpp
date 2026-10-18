/*
 * NULL JSFX - Clipper: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullclipper {

static const char* const kOversamplingOptions[] = { "OFF", "2X", "4X", "8X" };
static const char* const kLinkOptions[] = { "STEREO", "DUAL" };

// slider1..slider7 of clipper.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Input", "input", -12.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Ceiling", "ceiling", -24.0f, 0.0f, -0.1f, "dB", 0.1f },
    { "Knee", "knee", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Oversampling", "oversampling", 0.0f, 3.0f, 2.0f, "", 1.0f, kOversamplingOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Link", "link", 0.0f, 1.0f, 1.0f, "", 1.0f, kLinkOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { -12.0, 24.0, 0.0, 0.1 },
    { -24.0, 0.0, -0.1, 0.1 },
    { 0.0, 100.0, 0.0, 1.0 },
    { 0.0, 3.0, 2.0, 1.0 },
    { 0.0, 100.0, 100.0, 1.0 },
    { -24.0, 12.0, 0.0, 0.1 },
    { 0.0, 1.0, 1.0, 1.0 },
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

} // namespace nullclipper
