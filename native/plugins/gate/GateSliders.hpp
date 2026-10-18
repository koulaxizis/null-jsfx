/*
 * NULL JSFX - Gate: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullgate {

static const char* const kSidechain[] = { "INT", "EXT" };
static const char* const kOffOn[] = { "OFF", "ON" };

// slider1..slider10 of gate.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", -60.0f, 0.0f, -40.0f, "dB", 0.1f },
    { "Attack", "attack", 1.0f, 100.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 50.0f, 1000.0f, 200.0f, "ms", 1.0f },
    { "Sidechain HPF", "sc_hpf", 0.0f, 500.0f, 0.0f, "Hz", 1.0f },
    { "Range", "range", -80.0f, 0.0f, -80.0f, "dB", 0.1f },
    { "Hold", "hold", 0.0f, 500.0f, 0.0f, "ms", 1.0f },
    { "Lookahead", "lookahead", 0.0f, 10.0f, 0.0f, "ms", 0.1f },
    { "Sidechain", "sidechain", 0.0f, 1.0f, 0.0f, "", 1.0f, kSidechain },
    { "Sidechain LPF", "sc_lpf", 1000.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Flip", "flip", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullgate
