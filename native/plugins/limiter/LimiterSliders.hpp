/*
 * NULL JSFX - Limiter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulllimiter {

static const char* const kModeOptions[] = { "LIMITER", "CLIPPER", "BRICK" };
static const char* const kOnOptions[] = { "OFF", "ON" };
static const char* const kListenOptions[] = { "OUTPUT", "DELTA" };

// slider1..slider7 of limiter.jsfx, in order (6..7 appended in 2.7)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Threshold", "threshold", -30.0f, 0.0f, 0.0f, "dB", 0.1f },
    { "Ceiling", "ceiling", -6.0f, -0.1f, -0.1f, "dB", 0.1f },
    { "Release", "release", 0.0f, 1000.0f, 200.0f, "ms", 1.0f },
    { "Lookahead", "lookahead", 0.0f, 10.0f, 2.0f, "ms", 0.1f },
    { "True Peak", "true_peak", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOptions },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulllimiter
