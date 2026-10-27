/*
 * NULL JSFX - De-Clip: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulldeclip {

static const char* const kListenOptions[] = { "OUTPUT", "DELTA" };

// slider1..slider4 of de_clip.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Clip Level", "clip_level", -24.0f, 0.0f, -0.3f, "dB", 0.1f },
    { "Min Length", "min_length", 2.0f, 8.0f, 3.0f, "samples", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nulldeclip
