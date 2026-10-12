/*
 * NULL JSFX - Filter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullfilter {

static const char* const kModeOptions[] = { "LO PASS", "HI PASS", "BAND", "NOTCH" };

// slider1..slider4 of filter.jsfx, in order (Cutoff is log-shaped in the JSFX; the interface
// gives its knob a log taper)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Cutoff", "cutoff", 20.0f, 20000.0f, 1000.0f, "Hz", 1.0f },
    { "Resonance", "resonance", 0.1f, 10.0f, 1.0f, "", 0.1f },
    { "Gain", "gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mode", "mode", 0.0f, 3.0f, 0.0f, "", 1.0f, kModeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullfilter
