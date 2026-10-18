/*
 * NULL JSFX - Shaper: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullshaper {

static const char* const kShapeOptions[] = { "CUSTOM", "SOFT", "HARD", "TUBE", "FOLD", "SINE", "STEPS", "ASYM" };
static const char* const kSymmetryOptions[] = { "SYM", "ASYM" };
static const char* const kOversamplingOptions[] = { "OFF", "2X", "4X" };

// slider1..slider24 of shaper.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Shape", "shape", 0.0f, 7.0f, 1.0f, "", 1.0f, kShapeOptions },
    { "Drive", "drive", 0.0f, 36.0f, 6.0f, "dB", 0.1f },
    { "Bias", "bias", -50.0f, 50.0f, 0.0f, "%", 1.0f },
    { "Symmetry", "symmetry", 0.0f, 1.0f, 0.0f, "", 1.0f, kSymmetryOptions },
    { "Oversampling", "oversampling", 0.0f, 2.0f, 1.0f, "", 1.0f, kOversamplingOptions },
    { "Tone", "tone", 1000.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, -6.0f, "dB", 0.1f },
    { "P1 X", "p1x", 0.0f, 1.0f, 0.1f, "", 0.001f },
    { "P1 Y", "p1y", -1.0f, 1.0f, 0.205f, "", 0.001f },
    { "P2 X", "p2x", 0.0f, 1.0f, 0.25f, "", 0.001f },
    { "P2 Y", "p2y", -1.0f, 1.0f, 0.479f, "", 0.001f },
    { "P3 X", "p3x", 0.0f, 1.0f, 0.4f, "", 0.001f },
    { "P3 Y", "p3y", -1.0f, 1.0f, 0.689f, "", 0.001f },
    { "P4 X", "p4x", 0.0f, 1.0f, 0.6f, "", 0.001f },
    { "P4 Y", "p4y", -1.0f, 1.0f, 0.865f, "", 0.001f },
    { "P5 X", "p5x", 0.0f, 1.0f, 0.8f, "", 0.001f },
    { "P5 Y", "p5y", -1.0f, 1.0f, 0.956f, "", 0.001f },
    { "P6 X", "p6x", 0.0f, 1.0f, 1.0f, "", 0.001f },
    { "P6 Y", "p6y", -1.0f, 1.0f, 1.0f, "", 0.001f },
    { "P7 X", "p7x", 0.0f, 1.0f, 0.0f, "", 0.001f },
    { "P7 Y", "p7y", -1.0f, 1.0f, 0.0f, "", 0.001f },
    { "P8 X", "p8x", 0.0f, 1.0f, 0.0f, "", 0.001f },
    { "P8 Y", "p8y", -1.0f, 1.0f, 0.0f, "", 0.001f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);
static constexpr uint32_t kFirstPoint = 8; // P1 X; the points are x, y pairs from here

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 7.0, 1.0, 1.0 },
    { 0.0, 36.0, 6.0, 0.1 },
    { -50.0, 50.0, 0.0, 1.0 },
    { 0.0, 1.0, 0.0, 1.0 },
    { 0.0, 2.0, 1.0, 1.0 },
    { 1000.0, 20000.0, 20000.0, 1.0 },
    { 0.0, 100.0, 100.0, 1.0 },
    { -24.0, 12.0, 0.0, 0.1 },
    { 0.0, 1.0, 0.1, 0.001 },
    { -1.0, 1.0, 0.205, 0.001 },
    { 0.0, 1.0, 0.25, 0.001 },
    { -1.0, 1.0, 0.479, 0.001 },
    { 0.0, 1.0, 0.4, 0.001 },
    { -1.0, 1.0, 0.689, 0.001 },
    { 0.0, 1.0, 0.6, 0.001 },
    { -1.0, 1.0, 0.865, 0.001 },
    { 0.0, 1.0, 0.8, 0.001 },
    { -1.0, 1.0, 0.956, 0.001 },
    { 0.0, 1.0, 1.0, 0.001 },
    { -1.0, 1.0, 1.0, 0.001 },
    { 0.0, 1.0, 0.0, 0.001 },
    { -1.0, 1.0, 0.0, 0.001 },
    { 0.0, 1.0, 0.0, 0.001 },
    { -1.0, 1.0, 0.0, 0.001 },
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

} // namespace nullshaper
