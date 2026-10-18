/*
 * NULL JSFX - DeEsser: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulldeesser {

static const char* const kMode[] = { "SPLIT", "WIDE" };
static const char* const kThrMode[] = { "AUTO", "MANUAL" };
static const char* const kOffOn[] = { "OFF", "ON" };

// slider1..slider7 of deesser.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Target", "target", 4000.0f, 10000.0f, 6000.0f, "Hz", 100.0f },
    { "Reduction", "reduction", -18.0f, 0.0f, -6.0f, "dB", 0.1f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kMode },
    { "Threshold Mode", "threshold_mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kThrMode },
    { "Threshold", "threshold", -60.0f, 0.0f, -30.0f, "dB", 0.1f },
    { "Range", "range", -18.0f, 0.0f, -18.0f, "dB", 0.1f },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulldeesser
