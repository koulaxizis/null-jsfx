/*
 * NULL JSFX - Surround Panner: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullpan {

static const char* const kFormatOptions[] = { "5.1", "7.1", "7.1.4" };
static const char* const kInputOptions[] = { "MONO", "STEREO" };

// slider1..slider8 of surround_panner.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Format", "format", 0.0f, 2.0f, 0.0f, "", 1.0f, kFormatOptions },
    { "Input", "input", 0.0f, 1.0f, 1.0f, "", 1.0f, kInputOptions },
    { "Azimuth", "azimuth", -180.0f, 180.0f, 0.0f, "deg", 1.0f },
    { "Elevation", "elevation", 0.0f, 90.0f, 0.0f, "deg", 1.0f },
    { "Spread", "spread", 0.0f, 180.0f, 60.0f, "deg", 1.0f },
    { "Center", "center", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "LFE", "lfe", -60.0f, 0.0f, -60.0f, "dB", 0.1f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the bed's pins, as the JSFX names them
static const char* const kPinNames[12] = { "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr" };

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullpan
