/*
 * NULL JSFX - Cabinet Simulator: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullcab {

static const char* const kCabOptions[] = { "1X12", "2X12", "4X12", "1X15", "RADIO" };
// the full names, shown in the display
static const char* const kCabNames[] = { "1x12 Open Back", "2x12 Combo", "4x12 Closed Back", "1x15 Bass", "Small Radio" };

// slider1..slider5 of cabinet_simulator.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Cabinet", "cabinet", 0.0f, 4.0f, 2.0f, "", 1.0f, kCabOptions },
    { "Mic Position", "mic_position", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Low Cut", "low_cut", 20.0f, 300.0f, 80.0f, "Hz", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullcab
