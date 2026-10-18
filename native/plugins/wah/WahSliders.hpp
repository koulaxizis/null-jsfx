/*
 * NULL JSFX - Wah: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullwah {

static const char* const kModeOptions[] = { "Manual", "Auto", "LFO" };
static const char* const kModelOptions[] = { "Classic", "Vocal", "Wide" };
static const char* const kSyncOptions[] = { "Off", "1/1", "1/2", "1/4", "1/8", "1/16" };
static const char* const kModeShort[] = { "MANUAL", "AUTO", "LFO" };
static const char* const kModelShort[] = { "CLASSIC", "VOCAL", "WIDE" };
static const char* const kSyncShort[] = { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16" };

// slider1..slider11 of wah.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Pedal", "pedal", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mode", "mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Model", "model", 0.0f, 2.0f, 0.0f, "", 1.0f, kModelOptions },
    { "Resonance", "resonance", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Sensitivity", "sensitivity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Attack", "attack", 1.0f, 100.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 20.0f, 1000.0f, 150.0f, "ms", 1.0f },
    { "Rate", "rate", 0.1f, 10.0f, 2.0f, "Hz", 0.1f },
    { "Tempo Sync", "sync", 0.0f, 5.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selectors' upper-case names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Mode", "mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kModeShort },
    { "Model", "model", 0.0f, 2.0f, 0.0f, "", 1.0f, kModelShort },
    kSliders[3], kSliders[4], kSliders[5], kSliders[6], kSliders[7],
    { "Tempo Sync", "sync", 0.0f, 5.0f, 0.0f, "", 1.0f, kSyncShort },
    kSliders[9], kSliders[10],
};

// The same ranges in double, as the JSFX declares them.
struct Range { double min, max, def, step; };
static const Range kRanges[] = {
    { 0.0, 100.0, 50.0, 1.0 },
    { 0.0, 2.0, 0.0, 1.0 },
    { 0.0, 2.0, 0.0, 1.0 },
    { 0.0, 100.0, 60.0, 1.0 },
    { 0.0, 100.0, 50.0, 1.0 },
    { 1.0, 100.0, 10.0, 1.0 },
    { 20.0, 1000.0, 150.0, 1.0 },
    { 0.1, 10.0, 2.0, 0.1 },
    { 0.0, 5.0, 0.0, 1.0 },
    { 0.0, 100.0, 100.0, 1.0 },
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

} // namespace nullwah
