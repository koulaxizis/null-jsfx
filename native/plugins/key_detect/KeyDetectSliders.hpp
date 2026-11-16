/*
 * NULL JSFX - Key Detect: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "KeyDetectDSP.hpp"

namespace nullkeydetect {

static const char* const kOpt2[] = { "Krumhansl", "Temperley" };
static const char* const kOpt3[] = { "Off", "On" };

// slider1..slider4 of key_detect.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Window", "window", 2.0f, 60.0f, 10.0f, "s", 1.0f },
    { "Threshold", "threshold", -80.0f, -20.0f, -50.0f, "dB", 1.0f },
    { "Profile", "profile", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt2 },
    { "Hold", "hold", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt3 },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi2[] = { "KRUMHANSL", "TEMPERLEY" };
static const char* const kUi3[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    kSliders[1],
    { "Profile", "profile", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi2 },
    { "Hold", "hold", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi3 },
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.window = v(0);
    p.threshold = v(1);
    p.profile = int(v(2));
    p.hold = int(v(3));
    return p;
}

} // namespace nullkeydetect
