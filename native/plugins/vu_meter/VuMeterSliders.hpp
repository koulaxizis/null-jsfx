/*
 * NULL JSFX - VU Meter: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "VuMeterDSP.hpp"

namespace nullvu {

static const char* const kModeOptions[] = { "VU", "PPM", "K-14", "K-20" };
static const char* const kHoldOptions[] = { "OFF", "3 S", "HOLD" };
static const char* const kMonitorOptions[] = { "STEREO", "MONO", "SIDE" };

// slider1..slider4 of vu_meter.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 3.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Reference", "reference", -24.0f, -8.0f, -18.0f, "dBFS", 1.0f },
    { "Peak Hold", "peak_hold", 0.0f, 2.0f, 1.0f, "", 1.0f, kHoldOptions },
    { "Monitor", "monitor", 0.0f, 2.0f, 0.0f, "", 1.0f, kMonitorOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullvu

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the needles
class VuMeterBase : public NullMultiPlugin
{
public:
    VuMeterBase() : NullMultiPlugin(nullvu::kSliders, nullvu::kNumSliders) {}
    nullvu::View view;
};

END_NAMESPACE_DISTRHO
