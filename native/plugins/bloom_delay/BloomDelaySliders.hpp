/*
 * NULL JSFX - Bloom Delay: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullbloom {

static const char* const kSyncOptions[] = { "OFF", "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2", "1/1" };

// slider1..slider9 of bloom_delay.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Time", "time", 20.0f, 2000.0f, 375.0f, "ms", 1.0f },
    { "Sync", "sync", 0.0f, 7.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Feedback", "feedback", 0.0f, 100.0f, 45.0f, "%", 1.0f },
    { "Bloom", "bloom", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mod", "mod", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Damping", "damping", 1000.0f, 20000.0f, 7000.0f, "Hz", 1.0f },
    { "Width", "width", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 35.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullbloom
