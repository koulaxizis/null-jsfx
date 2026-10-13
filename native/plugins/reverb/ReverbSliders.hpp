/*
 * NULL JSFX - Reverb: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullreverb {

static const char* const kBypassOptions[] = { "OFF", "ON" };

// slider1..slider5 of reverb.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Room Size", "room_size", 10.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Damping", "damping", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Bypass", "bypass", 0.0f, 1.0f, 0.0f, "", 1.0f, kBypassOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullreverb
