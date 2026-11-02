/*
 * NULL JSFX - String Ensemble: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullstrings {

static const char* const kModeOptions[] = { "Classic", "Wide", "Deep" };
static const char* const kModeShort[] = { "CLASSIC", "WIDE", "DEEP" };

// slider1..slider9 of string_ensemble.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Violins 4'", "violins", 0.0f, 100.0f, 80.0f, "%", 1.0f },
    { "Violas 8'", "violas", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Cellos 16'", "cellos", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Attack", "attack", 5.0f, 3000.0f, 250.0f, "ms", 1.0f },
    { "Release", "release", 20.0f, 5000.0f, 900.0f, "ms", 1.0f },
    { "Brightness", "brightness", 0.0f, 100.0f, 55.0f, "%", 1.0f },
    { "Ensemble", "ensemble", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Ensemble Mode", "ensemble_mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Volume", "volume", -24.0f, 12.0f, -6.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullstrings
