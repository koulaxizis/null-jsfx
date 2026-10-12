/*
 * NULL JSFX - Vinyl Age: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullvinyl {

// slider1..slider8 of vinyl_age.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Wow", "wow", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Flutter", "flutter", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Crackle", "crackle", 0.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Noise Level", "noise", 0.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Bit Depth", "bits", 4.0f, 16.0f, 12.0f, "", 1.0f },
    { "Sample Rate", "rate", 2000.0f, 44100.0f, 22050.0f, "Hz", 100.0f },
    { "Lowpass Freq", "lowpass", 1000.0f, 20000.0f, 8000.0f, "Hz", 100.0f },
    { "Mix", "mix", 0.0f, 100.0f, 80.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullvinyl
