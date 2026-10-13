/*
 * NULL JSFX - Pitch Shifter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullpitchshifter {

// slider1..slider4 of pitch_shifter.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Octave", "octave", -2.0f, 2.0f, 0.0f, "oct", 1.0f },
    { "Semitones", "semitones", -12.0f, 12.0f, 0.0f, "st", 1.0f },
    { "Fine", "fine", -50.0f, 50.0f, 0.0f, "cents", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a parameter value on the JSFX slider's grid in double (min + n * step, at most max), as the
// JSFX sees it: a float such as 0.3f would otherwise detune the LFOs slightly
inline double onGrid(float v, double min, double max, double step)
{
    return std::fmin(max, min + std::round((double(v) - min) / step) * step);
}

} // namespace nullpitchshifter
