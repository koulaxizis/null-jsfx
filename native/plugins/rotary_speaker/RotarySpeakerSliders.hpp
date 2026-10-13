/*
 * NULL JSFX - Rotary Speaker: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "RotarySpeakerDSP.hpp"

#include <cmath>

namespace nullrotary {

static const char* const kSpeedOptions[] = { "Slow (Chorale)", "Fast (Tremolo)", "Stop" };
// the same choices shortened to fit a selector cell in the interface
static const char* const kSpeedShort[] = { "SLOW", "FAST", "STOP" };

#define NULL_ROTARY_SLIDERS(speedOptions)                                         \
    { "Speed", "speed", 0.0f, 2.0f, 0.0f, "", 1.0f, speedOptions },              \
    { "Acceleration", "acceleration", 10.0f, 400.0f, 100.0f, "%", 1.0f },        \
    { "Horn/Drum Balance", "balance", 0.0f, 100.0f, 50.0f, "%", 1.0f },          \
    { "Drive", "drive", 0.0f, 100.0f, 20.0f, "%", 1.0f },                         \
    { "Width", "width", 0.0f, 100.0f, 80.0f, "%", 1.0f },                         \
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },

// slider1..slider6 of rotary_speaker.jsfx, in order (the plugin's parameters)
static const DISTRHO::SliderInfo kSliders[] = { NULL_ROTARY_SLIDERS(kSpeedOptions) };
// the same for the interface, with the short Speed labels
static const DISTRHO::SliderInfo kUiSliders[] = { NULL_ROTARY_SLIDERS(kSpeedShort) };
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

#undef NULL_ROTARY_SLIDERS

// the parameter values as DSP settings
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    p.speed = int(std::lround(value(0)));
    p.accelPct = std::round(value(1));
    p.balancePct = std::round(value(2));
    p.drivePct = std::round(value(3));
    p.widthPct = std::round(value(4));
    p.mixPct = std::round(value(5));
    return p;
}

} // namespace nullrotary
