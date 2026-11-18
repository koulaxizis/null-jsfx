/*
 * NULL JSFX - Band Spin: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullbandspin {

// slider1..slider12 of band_spin.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Crossover 1", "crossover_1", 30.0f, 1000.0f, 200.0f, "Hz", 1.0f },
    { "Crossover 2", "crossover_2", 200.0f, 6000.0f, 1200.0f, "Hz", 1.0f },
    { "Crossover 3", "crossover_3", 1000.0f, 16000.0f, 5000.0f, "Hz", 1.0f },
    { "Speed 1", "speed_1", 0.05f, 10.0f, 0.6f, "Hz", 0.01f },
    { "Speed 2", "speed_2", 0.05f, 10.0f, 1.1f, "Hz", 0.01f },
    { "Speed 3", "speed_3", 0.05f, 10.0f, 2.3f, "Hz", 0.01f },
    { "Speed 4", "speed_4", 0.05f, 10.0f, 4.7f, "Hz", 0.01f },
    { "Volume", "volume", 0.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Pan", "pan", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Doppler", "doppler", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Follow", "follow", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the values as the JSFX holds them (Speed in 0.01 steps)
inline void readSliders(const float* v, double* s) noexcept
{
    for (uint32_t i = 0; i < kNumSliders; ++i)
        s[i] = v[i];
    for (int i = 3; i < 7; ++i)
        s[i] = std::round(s[i] * 100.0) / 100.0;
}

} // namespace nullbandspin
