/*
 * NULL JSFX - Phase Align: the JSFX sliders, shared by the plugin, the interface and the Rack module
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullphase {

static const char* const kSourceOptions[] = { "R TO L", "SIDECHAIN" };
static const char* const kOffOnOptions[] = { "OFF", "ON" };
static const char* const kListenOptions[] = { "NORMAL", "MONO SUM" };

// slider1..slider8 of phase_align.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Source", "source", 0.0f, 1.0f, 0.0f, "", 1.0f, kSourceOptions },
    { "Delay", "delay", -10.0f, 10.0f, 0.0f, "ms", 0.01f },
    { "Fine", "fine", -1.0f, 1.0f, 0.0f, "samples", 0.01f },
    { "Invert Left", "invert_left", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOnOptions },
    { "Invert Right", "invert_right", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOnOptions },
    { "Rotation", "rotation", 0.0f, 360.0f, 0.0f, "deg", 1.0f },
    { "Focus", "focus", 20.0f, 1000.0f, 100.0f, "Hz", 1.0f },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The host passes parameters as float, the JSFX keeps the slider in double: the exact ends and
// default, else the nearest point of the slider's step grid (min + k * step, as the JSFX's)
inline double sliderValue(uint32_t i, float v) noexcept
{
    if (i >= kNumSliders)
        return double(v);
    const DISTRHO::SliderInfo& s = kSliders[i];
    const double lo = double(s.min), hi = double(s.max);
    const double st = s.step >= 1.0f ? 1.0 : 0.01;
    if (v <= s.min)
        return lo;
    if (v >= s.max)
        return hi;
    if (v == s.def)
        return double(s.def);
    return std::fmax(lo, std::fmin(hi, lo + std::round((double(v) - lo) / st) * st));
}

} // namespace nullphase
