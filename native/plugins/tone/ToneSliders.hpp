/*
 * NULL JSFX - Tone: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulltone {

// slider1..slider5 of tone.jsfx, in order (the two frequencies are log-shaped in the JSFX; the
// interface gives their knobs a log taper)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Bass", "bass", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Treble", "treble", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Bass Freq", "bass_freq", 40.0f, 400.0f, 100.0f, "Hz", 1.0f },
    { "Treble Freq", "treble_freq", 2000.0f, 16000.0f, 8000.0f, "Hz", 10.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nulltone
