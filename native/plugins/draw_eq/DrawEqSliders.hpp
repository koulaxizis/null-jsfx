/*
 * NULL JSFX - Draw EQ: the JSFX sliders, shared by the plugin, the interface and the Rack module
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nulldraweq {

static constexpr int kPoints = 10;

// slider1..slider13 of draw_eq.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "31 Hz", "g31", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "63 Hz", "g63", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "125 Hz", "g125", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "250 Hz", "g250", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "500 Hz", "g500", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "1 kHz", "g1k", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "2 kHz", "g2k", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "4 kHz", "g4k", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "8 kHz", "g8k", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "16 kHz", "g16k", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "Smoothness", "smoothness", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Output", "output", -18.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);
static constexpr uint32_t kSmoothness = 10, kOutput = 11, kMix = 12;

// The host passes parameters as float, the JSFX keeps the slider in double: the nearest point of
// the slider's step grid (0.1 or 1), exact at the ends.
inline double sliderValue(uint32_t i, float v) noexcept
{
    if (i >= kNumSliders)
        return double(v);
    const DISTRHO::SliderInfo& s = kSliders[i];
    const double lo = double(s.min), hi = double(s.max);
    const double st = s.step >= 1.0f ? 1.0 : 0.1;
    if (v <= s.min)
        return lo;
    if (v >= s.max)
        return hi;
    return std::fmax(lo, std::fmin(hi, lo + std::round((double(v) - lo) / st) * st));
}

} // namespace nulldraweq
