/*
 * NULL JSFX - Glitch FX: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "GlitchFxDSP.hpp"

#include <cmath>

namespace nullglitch {

static const char* const kOnOff[] = { "OFF", "ON" };

// slider1..slider14 of glitch_fx.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Rate", "rate", 0.1f, 20.0f, 4.0f, "Hz", 0.1f },
    { "Stutter Length", "length", 1.0f, 64.0f, 8.0f, "", 1.0f },
    { "Stutter", "stutter", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Gate Depth", "gate_depth", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Gate", "gate", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Pitch Shift", "pitch_shift", -24.0f, 24.0f, 0.0f, "semitones", 0.1f },
    { "Pitch Shift On", "pitch", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Reverse Chance", "reverse_chance", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Reverse", "reverse", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Tape Stop Time", "tape_time", 10.0f, 1000.0f, 300.0f, "ms", 10.0f },
    { "Tape Stop", "tape_stop", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Grain Size", "grain_size", 10.0f, 500.0f, 200.0f, "ms", 10.0f },
    { "Granular", "granular", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a 0.1-step slider value as the JSFX sees it (the double nearest the decimal), not the float
inline double tenths(float v) { return std::round(double(v) * 10.0) / 10.0; }

// the parameter values as DSP settings
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    p.rate = tenths(value(0));
    p.lengthDiv = std::round(value(1));
    p.stutter = value(2) >= 0.5f;
    p.gateDepth = std::round(value(3));
    p.gate = value(4) >= 0.5f;
    p.pitchSt = tenths(value(5));
    p.pitch = value(6) >= 0.5f;
    p.revChance = std::round(value(7));
    p.reverse = value(8) >= 0.5f;
    p.tapeMs = std::round(value(9));
    p.tape = value(10) >= 0.5f;
    p.grainMs = std::round(value(11));
    p.granular = value(12) >= 0.5f;
    p.mixPct = std::round(value(13));
    return p;
}

} // namespace nullglitch
