/*
 * NULL JSFX - Kick Shaper: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "KickShaperDSP.hpp"

#include <cmath>

namespace nullkick {

// slider1..slider6 of kick_shaper.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Attack Shape", "attack", -20.0f, 20.0f, 0.0f, "dB", 0.1f },
    { "Body Tone", "body", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Thump", "thump", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Click", "click", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Decay", "decay", 50.0f, 500.0f, 250.0f, "ms", 1.0f },
    { "Output Gain", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a 0.1-step slider value as the JSFX sees it (the double nearest the decimal), not the float
inline double tenths(float v) { return std::round(double(v) * 10.0) / 10.0; }

// the parameter values as DSP settings
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    p.attackDb = tenths(value(0));
    p.bodyDb = tenths(value(1));
    p.thumpDb = tenths(value(2));
    p.clickDb = tenths(value(3));
    p.decayMs = std::round(value(4));
    p.outDb = tenths(value(5));
    return p;
}

} // namespace nullkick
