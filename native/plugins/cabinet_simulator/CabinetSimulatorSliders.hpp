/*
 * NULL JSFX - Cabinet Simulator: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullcab {

static const char* const kCabOptions[] = { "1X12", "2X12", "4X12", "1X15", "RADIO", "1X10", "2X10", "4X10" };
// the full names, shown in the display
static const char* const kCabNames[] = { "1x12 Open Back", "2x12 Combo", "4x12 Closed Back", "1x15 Bass", "Small Radio",
                                             "1x10 Tweed", "2x10 Bass", "4x10 Vintage" };

// slider1..slider7 of cabinet_simulator.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Cabinet", "cabinet", 0.0f, 7.0f, 2.0f, "", 1.0f, kCabOptions },
    { "Mic Position", "mic_position", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Low Cut", "low_cut", 20.0f, 300.0f, 80.0f, "Hz", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mic 2 Position", "mic2_position", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Mic Blend", "mic_blend", 0.0f, 100.0f, 0.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullcab
