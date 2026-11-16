/*
 * NULL JSFX - Lo-Fi Comp parameters, shared by Plugin.cpp, UI.cpp and the Rack module
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulllofi {

// slider1..slider7 of lofi_comp.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Squash", "squash", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Release", "release", 20.0f, 1000.0f, 150.0f, "ms", 1.0f },
    { "Sample Rate", "sample_rate", 4000.0f, 48000.0f, 32000.0f, "Hz", 10.0f },
    { "Bits", "bits", 4.0f, 16.0f, 12.0f, "", 1.0f },
    { "Wow", "wow", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulllofi
