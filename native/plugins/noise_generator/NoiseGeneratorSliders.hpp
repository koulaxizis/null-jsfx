/*
 * NULL JSFX - Noise Generator: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullnoise {

static const char* const kTypeOptions[] = { "WHITE", "PINK", "BROWN" };

// slider1..slider3 of noise_generator.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Type", "type", 0.0f, 2.0f, 0.0f, "", 1.0f, kTypeOptions },
    { "Level", "level", -60.0f, 0.0f, -24.0f, "dB", 0.1f },
    { "Stereo Width", "width", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullnoise
