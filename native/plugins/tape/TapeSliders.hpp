/*
 * NULL JSFX - Tape: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nulltape {

static const char* const kSpeedOptions[] = { "7.5 IPS", "15 IPS", "30 IPS" };
static const char* const kTypeOptions[] = { "CLASSIC", "TYPE I", "TYPE II", "TYPE IV" };
static const char* const kNrOptions[] = { "OFF", "ON" };
static const char* const kOsOptions[] = { "OFF", "2X", "4X" };

// slider1..slider11 of tape.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Drive", "drive", 0.0f, 24.0f, 3.0f, "dB", 0.1f },
    { "Speed", "speed", 0.0f, 2.0f, 1.0f, "", 1.0f, kSpeedOptions },
    { "Head Bump", "bump", 0.0f, 9.0f, 3.0f, "dB", 0.1f },
    { "Wow", "wow", 0.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Flutter", "flutter", 0.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Hiss", "hiss", 0.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Tape Type", "tape_type", 0.0f, 3.0f, 0.0f, "", 1.0f, kTypeOptions },
    { "Noise Reduction", "noise_reduction", 0.0f, 1.0f, 0.0f, "", 1.0f, kNrOptions },
    { "Oversampling", "oversampling", 0.0f, 2.0f, 0.0f, "", 1.0f, kOsOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 24.0, 3.0, 0.1 },
    { 0.0, 2.0, 1.0, 1.0 },
    { 0.0, 9.0, 3.0, 0.1 },
    { 0.0, 100.0, 10.0, 1.0 },
    { 0.0, 100.0, 10.0, 1.0 },
    { 0.0, 100.0, 10.0, 1.0 },
    { -12.0, 12.0, 0.0, 0.1 },
    { 0.0, 100.0, 100.0, 1.0 },
    { 0.0, 3.0, 0.0, 1.0 },
    { 0.0, 1.0, 0.0, 1.0 },
    { 0.0, 2.0, 0.0, 1.0 },
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

} // namespace nulltape
