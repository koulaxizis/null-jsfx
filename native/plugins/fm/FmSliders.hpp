/*
 * NULL JSFX - FM: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullfm {

static const char* const kAlgorithmOptions[] = { "Stack", "Dual Mod", "Branch", "Pair Stack", "Two Pairs", "Triple", "One Pair", "Additive" };
static const char* const kAlgorithmShort[] = { "STACK", "DUAL MOD", "BRANCH", "PAIR STACK", "TWO PAIRS", "TRIPLE", "ONE PAIR", "ADDITIVE" };
static const char* const kMpeOptions[] = { "Off", "On" };
static const char* const kMpeShort[] = { "OFF", "ON" };

// slider1..slider37 of fm.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Algorithm", "algorithm", 0.0f, 7.0f, 0.0f, "", 1.0f, kAlgorithmOptions },
    { "Feedback", "feedback", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Op 1 Ratio", "op_1_ratio", 0.5f, 16.0f, 1.0f, "", 0.01f },
    { "Op 1 Level", "op_1_level", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Op 1 Attack", "op_1_attack", 1.0f, 10000.0f, 2.0f, "ms", 1.0f },
    { "Op 1 Decay", "op_1_decay", 1.0f, 10000.0f, 1500.0f, "ms", 1.0f },
    { "Op 1 Sustain", "op_1_sustain", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Op 1 Release", "op_1_release", 1.0f, 10000.0f, 400.0f, "ms", 1.0f },
    { "Op 1 Velocity", "op_1_velocity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Op 2 Ratio", "op_2_ratio", 0.5f, 16.0f, 1.0f, "", 0.01f },
    { "Op 2 Level", "op_2_level", 0.0f, 100.0f, 45.0f, "%", 1.0f },
    { "Op 2 Attack", "op_2_attack", 1.0f, 10000.0f, 2.0f, "ms", 1.0f },
    { "Op 2 Decay", "op_2_decay", 1.0f, 10000.0f, 800.0f, "ms", 1.0f },
    { "Op 2 Sustain", "op_2_sustain", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Op 2 Release", "op_2_release", 1.0f, 10000.0f, 400.0f, "ms", 1.0f },
    { "Op 2 Velocity", "op_2_velocity", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Op 3 Ratio", "op_3_ratio", 0.5f, 16.0f, 2.0f, "", 0.01f },
    { "Op 3 Level", "op_3_level", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Op 3 Attack", "op_3_attack", 1.0f, 10000.0f, 2.0f, "ms", 1.0f },
    { "Op 3 Decay", "op_3_decay", 1.0f, 10000.0f, 800.0f, "ms", 1.0f },
    { "Op 3 Sustain", "op_3_sustain", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Op 3 Release", "op_3_release", 1.0f, 10000.0f, 400.0f, "ms", 1.0f },
    { "Op 3 Velocity", "op_3_velocity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Op 4 Ratio", "op_4_ratio", 0.5f, 16.0f, 1.0f, "", 0.01f },
    { "Op 4 Level", "op_4_level", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Op 4 Attack", "op_4_attack", 1.0f, 10000.0f, 2.0f, "ms", 1.0f },
    { "Op 4 Decay", "op_4_decay", 1.0f, 10000.0f, 800.0f, "ms", 1.0f },
    { "Op 4 Sustain", "op_4_sustain", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Op 4 Release", "op_4_release", 1.0f, 10000.0f, 400.0f, "ms", 1.0f },
    { "Op 4 Velocity", "op_4_velocity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Brightness", "brightness", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Chorus Mix", "chorus_mix", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Reverb Mix", "reverb_mix", 0.0f, 100.0f, 15.0f, "%", 1.0f },
    { "Reverb Size", "reverb_size", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Octave", "octave", -3.0f, 3.0f, 0.0f, "", 1.0f },
    { "Bend Range", "bend_range", 0.0f, 24.0f, 2.0f, "st", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "MPE", "mpe", 0.0f, 1.0f, 0.0f, "", 1.0f, kMpeOptions },
    { "MPE Bend Range", "mpe_bend_range", 0.0f, 96.0f, 48.0f, "st", 1.0f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullfm
