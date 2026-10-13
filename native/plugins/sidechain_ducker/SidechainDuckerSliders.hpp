/*
 * NULL JSFX - Sidechain Ducker: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullducker {

static const char* const kKeyOptions[] = { "SIDE", "MAIN" };

// slider1..slider7 of sidechain_ducker.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", -60.0f, 0.0f, -30.0f, "dB", 0.1f },
    { "Depth", "depth", -40.0f, 0.0f, -12.0f, "dB", 0.1f },
    { "Attack", "attack", 0.1f, 50.0f, 2.0f, "ms", 0.1f },
    { "Hold", "hold", 0.0f, 500.0f, 50.0f, "ms", 1.0f },
    { "Release", "release", 10.0f, 2000.0f, 200.0f, "ms", 1.0f },
    { "Key", "key", 0.0f, 1.0f, 0.0f, "", 1.0f, kKeyOptions },
    { "Key High Pass", "key_hp", 0.0f, 500.0f, 0.0f, "Hz", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullducker
