/*
 * NULL JSFX - Channel Utility: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <cmath>

namespace nullchannelutility {

static const char* const kPadOptions[] = { "NONE", "-6 dB", "-12 dB" };
static const char* const kOffOn[] = { "OFF", "ON" };
static const char* const kMonoOptions[] = { "STEREO", "MONO" };

// slider1..slider13 of channel_utility.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Input Pad", "input_pad", 0.0f, 2.0f, 0.0f, "", 1.0f, kPadOptions },
    { "Input Gain", "input_gain", -12.0f, 18.0f, 0.0f, "dB", 0.1f },
    { "Phase Left", "phase_left", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Phase Right", "phase_right", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "DC Block Freq", "dc_freq", 10.0f, 100.0f, 20.0f, "Hz", 1.0f },
    { "DC Block Mix", "dc_mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Balance Left", "balance_left", -6.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Balance Right", "balance_right", -6.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Solo L", "solo_l", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Solo R", "solo_r", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Mono Check", "mono", 0.0f, 1.0f, 0.0f, "", 1.0f, kMonoOptions },
    { "Output Gain", "output_gain", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Hard Limit", "hard_limit", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a 0.1-step slider value as the JSFX sees it (the parameter arrives as a float); + 0.0 turns
// -0.0 into 0.0 for the value texts
inline double tenth(float v) noexcept { return std::round(double(v) * 10.0) / 10.0 + 0.0; }

} // namespace nullchannelutility
