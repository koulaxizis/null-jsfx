/*
 * NULL JSFX - Spectral Freeze: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullfreeze {

static const char* const kFreezeOptions[] = { "OFF", "FROZEN" };

// slider1..slider5 of spectral_freeze.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeOptions },
    { "Fade", "fade", 10.0f, 3000.0f, 300.0f, "ms", 1.0f },
    { "Smear", "smear", 0.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Level", "level", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullfreeze
