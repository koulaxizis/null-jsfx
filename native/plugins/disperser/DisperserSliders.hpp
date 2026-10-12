/*
 * NULL JSFX - Disperser: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <cmath>

namespace nulldisperser {

static const char* const kModeOptions[] = { "MONO", "STEREO" };

// slider1..slider5 of disperser.jsfx, in order (Frequency is log-shaped in the JSFX; the
// interface gives its knob a log taper)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Frequency", "frequency", 20.0f, 20000.0f, 1000.0f, "Hz", 1.0f },
    { "Amount", "amount", 0.0f, 64.0f, 16.0f, "", 1.0f },
    { "Pinch", "pinch", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The host passes parameters as float; every slider here has step 1, so the JSFX value is the
// nearest integer within the range.
inline double sliderValue(uint32_t i, float v) noexcept
{
    const DISTRHO::SliderInfo& s = kSliders[i];
    return std::fmax(double(s.min), std::fmin(double(s.max), std::round(double(v))));
}

} // namespace nulldisperser
