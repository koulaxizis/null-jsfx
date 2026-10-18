/*
 * NULL JSFX - Filter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullfilter {

static const char* const kModeOptions[] = { "LO PASS", "HI PASS", "BAND", "NOTCH" };
static const char* const kSlopeOptions[] = { "12 dB", "24 dB" };
static const char* const kSyncOptions[] = { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4D", "1/8D" };

// slider1..slider12 of filter.jsfx, in order (Cutoff, LFO Rate and Env Release are log-shaped
// in the JSFX; the interface gives their knobs a log taper)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Cutoff", "cutoff", 20.0f, 20000.0f, 1000.0f, "Hz", 1.0f },
    { "Resonance", "resonance", 0.1f, 10.0f, 1.0f, "", 0.1f },
    { "Gain", "gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Mode", "mode", 0.0f, 3.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Slope", "slope", 0.0f, 1.0f, 0.0f, "", 1.0f, kSlopeOptions },
    { "Drive", "drive", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "LFO Rate", "lfo_rate", 0.05f, 20.0f, 1.0f, "Hz", 0.01f },
    { "LFO Sync", "lfo_sync", 0.0f, 9.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "LFO Depth", "lfo_depth", 0.0f, 4.0f, 0.0f, "oct", 0.1f },
    { "Envelope Depth", "env_depth", -4.0f, 4.0f, 0.0f, "oct", 0.1f },
    { "Env Attack", "env_attack", 1.0f, 200.0f, 10.0f, "ms", 1.0f },
    { "Env Release", "env_release", 10.0f, 2000.0f, 150.0f, "ms", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullfilter
