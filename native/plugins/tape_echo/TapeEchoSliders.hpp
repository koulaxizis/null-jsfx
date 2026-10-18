/*
 * NULL JSFX - Tape Echo: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nulltapeecho {

static const char* const kSyncOptions[] = { "Off", "1/4", "1/8", "1/16", "1/4D", "1/8D", "1/4T", "1/8T" };
static const char* const kHeadsOptions[] = { "1", "2", "3", "1+2", "1+3", "2+3", "1+2+3",
                                              "1+R", "1+2+R", "2+3+R", "1+2+3+R", "R" };
static const char* const kFreezeOptions[] = { "Off", "On" };
static const char* const kSyncShort[] = { "OFF", "1/4", "1/8", "1/16", "1/4D", "1/8D", "1/4T", "1/8T" };
static const char* const kFreezeShort[] = { "OFF", "ON" };

// slider1..slider11 of tape_echo.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Repeat Time", "time", 50.0f, 1000.0f, 300.0f, "ms", 1.0f },
    { "Tempo Sync", "sync", 0.0f, 7.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Heads", "heads", 0.0f, 11.0f, 3.0f, "", 1.0f, kHeadsOptions },
    { "Intensity", "intensity", 0.0f, 110.0f, 40.0f, "%", 1.0f },
    { "Bass", "bass", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Treble", "treble", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Wow/Flutter", "wow_flutter", 0.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Saturation", "saturation", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Spring", "spring", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 35.0f, "%", 1.0f },
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selectors' upper-case names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Tempo Sync", "sync", 0.0f, 7.0f, 0.0f, "", 1.0f, kSyncShort },
    kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6], kSliders[7], kSliders[8], kSliders[9],
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeShort },
};

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 50.0, 1000.0, 300.0, 1.0 },
    { 0.0, 7.0, 0.0, 1.0 },
    { 0.0, 11.0, 3.0, 1.0 },
    { 0.0, 110.0, 40.0, 1.0 },
    { -12.0, 12.0, 0.0, 0.1 },
    { -12.0, 12.0, 0.0, 0.1 },
    { 0.0, 100.0, 25.0, 1.0 },
    { 0.0, 100.0, 30.0, 1.0 },
    { 0.0, 100.0, 0.0, 1.0 },
    { 0.0, 100.0, 35.0, 1.0 },
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

} // namespace nulltapeecho
