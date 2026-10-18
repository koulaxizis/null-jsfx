/*
 * NULL JSFX - Dynamics: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulldynamics {

static const char* const kModeOptions[] = { "COMP", "EXPAND" };
static const char* const kStyleOptions[] = { "CLEAN", "BUS", "FET", "OPTO", "UPWARD" };
static const char* const kSidechainOptions[] = { "INT", "EXT" };

// slider1..slider11 of dynamics.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Threshold", "threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
    { "Ratio", "ratio", 1.0f, 20.0f, 4.0f, "", 0.1f },
    { "Knee", "knee", 0.0f, 20.0f, 6.0f, "dB", 1.0f },
    { "Attack", "attack", 0.0f, 200.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 0.0f, 1000.0f, 100.0f, "ms", 1.0f },
    { "Makeup", "makeup", 0.0f, 20.0f, 0.0f, "dB", 1.0f },
    { "Style", "style", 0.0f, 4.0f, 0.0f, "", 1.0f, kStyleOptions },
    { "Sidechain HPF", "schpf", 20.0f, 500.0f, 20.0f, "Hz", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Sidechain", "sidechain", 0.0f, 1.0f, 0.0f, "", 1.0f, kSidechainOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// The Rack's copy: Style without UPWARD (0..3). Rack slot values are stored normalized (0..1 of the
// slider's range), so widening the range there would move saved FET and OPTO slots to OPTO and
// UPWARD; the Rack keeps the 2.3 range and older Rack sessions sound the same.
static const DISTRHO::SliderInfo kRackSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6],
    { "Style", "style", 0.0f, 3.0f, 0.0f, "", 1.0f, kStyleOptions },
    kSliders[8], kSliders[9], kSliders[10],
};
static_assert(sizeof(kRackSliders) / sizeof(kRackSliders[0]) == kNumSliders, "one Rack slider per JSFX slider");

} // namespace nulldynamics
