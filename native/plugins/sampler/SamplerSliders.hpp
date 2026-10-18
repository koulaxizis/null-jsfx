/*
 * NULL JSFX - Sampler: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * slider1 of the JSFX (Sample File, a file slider) is not a parameter here: the file is the
 * "sample" state (a path, picked in the interface). The parameters are slider2..slider21, in
 * order.
 */

#pragma once

#include "NullSliders.hpp"

namespace nullsmp {

static const char* const kModeOptions[] = { "One-shot", "Chromatic" };
static const char* const kModeShort[] = { "ONE-SHOT", "CHROMATIC" };
static const char* const kOffOnOptions[] = { "Off", "On" };
static const char* const kOffOnShort[] = { "OFF", "ON" };

static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 1.0f, "", 1.0f, kModeOptions },
    { "Root Key", "root_key", 0.0f, 127.0f, 60.0f, "", 1.0f },
    { "Transpose", "transpose", -24.0f, 24.0f, 0.0f, "st", 1.0f },
    { "Fine", "fine", -100.0f, 100.0f, 0.0f, "ct", 1.0f },
    { "Attack", "attack", 1.0f, 5000.0f, 1.0f, "ms", 1.0f },
    { "Decay", "decay", 1.0f, 5000.0f, 800.0f, "ms", 1.0f },
    { "Sustain", "sustain", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Release", "release", 1.0f, 5000.0f, 250.0f, "ms", 1.0f },
    { "Cutoff", "cutoff", 20.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Resonance", "resonance", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Filter Env", "filter_env", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Filter Decay", "filter_decay", 1.0f, 5000.0f, 300.0f, "ms", 1.0f },
    { "Loop", "loop", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOnOptions },
    { "Loop Start", "loop_start", 0.0f, 100.0f, 0.0f, "%", 0.1f },
    { "Loop End", "loop_end", 0.0f, 100.0f, 100.0f, "%", 0.1f },
    { "Start", "start", 0.0f, 100.0f, 0.0f, "%", 0.1f },
    { "Reverse", "reverse", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOnOptions },
    { "Velocity", "velocity", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Bend Range", "bend_range", 0.0f, 24.0f, 2.0f, "st", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullsmp
