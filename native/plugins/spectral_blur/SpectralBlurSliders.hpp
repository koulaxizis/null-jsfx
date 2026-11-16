/*
 * NULL JSFX - Spectral Blur: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullblur {

// slider1..slider6 of spectral_blur.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Smear", "smear", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Shift", "shift", -2000.0f, 2000.0f, 0.0f, "Hz", 1.0f },
    { "Tilt", "tilt", -6.0f, 6.0f, 0.0f, "dB/oct", 0.1f },
    { "Gate", "gate", -90.0f, 0.0f, -90.0f, "dB", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullblur
