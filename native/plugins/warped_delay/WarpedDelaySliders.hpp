/*
 * NULL JSFX - Warped Delay: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullwarped {

static const char* const kFilterOptions[] = { "OFF", "LP", "HP", "BP" };

// slider1..slider8 of warped_delay.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Time", "time", 1.0f, 1000.0f, 150.0f, "ms", 1.0f },
    { "Mod Rate", "rate", 0.1f, 10.0f, 2.0f, "Hz", 0.1f },
    { "Mod Depth", "depth", 0.0f, 50.0f, 10.0f, "ms", 0.1f },
    { "Feedback", "feedback", 0.0f, 99.0f, 40.0f, "%", 1.0f },
    { "Distortion", "distortion", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Filter", "filter", 0.0f, 3.0f, 0.0f, "", 1.0f, kFilterOptions },
    { "Filter Freq", "freq", 100.0f, 20000.0f, 2000.0f, "Hz", 100.0f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullwarped
