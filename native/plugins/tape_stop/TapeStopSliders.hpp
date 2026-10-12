/*
 * NULL JSFX - Tape Stop: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <cmath>

namespace nulltapestop {

static const char* const kTriggerOptions[] = { "IDLE", "STOP" };

// slider1..slider3 of tape_stop.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Trigger", "trigger", 0.0f, 1.0f, 0.0f, "", 1.0f, kTriggerOptions },
    { "Stop Time", "stop", 50.0f, 5000.0f, 500.0f, "ms", 10.0f },
    { "Start Time", "start", 50.0f, 5000.0f, 500.0f, "ms", 10.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 1.0, 0.0, 1.0 },
    { 50.0, 5000.0, 500.0, 10.0 },
    { 50.0, 5000.0, 500.0, 10.0 },
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

} // namespace nulltapestop
