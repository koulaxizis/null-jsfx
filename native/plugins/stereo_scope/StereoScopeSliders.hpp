/*
 * NULL JSFX - Stereo Scope: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "StereoScopeDSP.hpp"

namespace nullscope {

static const char* const kAutoOptions[] = { "OFF", "ON" };
static const char* const kMonitorOptions[] = { "STEREO", "MONO", "SIDE" };

// slider1..slider6 of stereo_scope.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Scope Gain", "scope_gain", -12.0f, 24.0f, 0.0f, "dB", 0.5f },
    { "Auto Gain", "auto_gain", 0.0f, 1.0f, 1.0f, "", 1.0f, kAutoOptions },
    { "Decay", "decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Integration", "integration", 50.0f, 3000.0f, 300.0f, "ms", 10.0f },
    { "Monitor", "monitor", 0.0f, 2.0f, 0.0f, "", 1.0f, kMonitorOptions },
    { "Band Correlation", "band_correlation", 0.0f, 1.0f, 0.0f, "", 1.0f, kAutoOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullscope

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the measurements
class StereoScopeBase : public NullMultiPlugin
{
public:
    StereoScopeBase() : NullMultiPlugin(nullscope::kSliders, nullscope::kNumSliders) {}
    nullscope::View view;
};

END_NAMESPACE_DISTRHO
