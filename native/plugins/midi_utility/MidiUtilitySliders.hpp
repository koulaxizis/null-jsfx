/*
 * NULL JSFX - MIDI Utility: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullmidiutil {

static const char* const kChannelOptions[] = { "Thru", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
static const char* const kPassOptions[] = { "Pass", "Block" };

// slider1..slider14 of midi_utility.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Transpose", "transpose", -48.0f, 48.0f, 0.0f, "st", 1.0f },
    { "Channel", "channel", 0.0f, 16.0f, 0.0f, "", 1.0f, kChannelOptions },
    { "Velocity Curve", "velocity_curve", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Velocity Min", "velocity_min", 1.0f, 127.0f, 1.0f, "", 1.0f },
    { "Velocity Max", "velocity_max", 1.0f, 127.0f, 127.0f, "", 1.0f },
    { "Fixed Velocity", "fixed_velocity", 0.0f, 127.0f, 0.0f, "", 1.0f },
    { "Key Low", "key_low", 0.0f, 127.0f, 0.0f, "", 1.0f },
    { "Key High", "key_high", 0.0f, 127.0f, 127.0f, "", 1.0f },
    { "Velocity Low", "velocity_low", 1.0f, 127.0f, 1.0f, "", 1.0f },
    { "Velocity High", "velocity_high", 1.0f, 127.0f, 127.0f, "", 1.0f },
    { "CC", "cc", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassOptions },
    { "Pitch Bend", "pitch_bend", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassOptions },
    { "Aftertouch", "aftertouch", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassOptions },
    { "Program Change", "program_change", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the selectors' short upper-case names, for the interface (as midi_utility_gfx.jsfx)
static const char* const kChannelShort[] = { "THRU", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
static const char* const kPassShort[] = { "PASS", "BLOCK" };

static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Channel", "channel", 0.0f, 16.0f, 0.0f, "", 1.0f, kChannelShort },
    kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6], kSliders[7], kSliders[8], kSliders[9],
    { "CC", "cc", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassShort },
    { "Pitch Bend", "pitch_bend", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassShort },
    { "Aftertouch", "aftertouch", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassShort },
    { "Program Change", "program_change", 0.0f, 1.0f, 0.0f, "", 1.0f, kPassShort },
};

} // namespace nullmidiutil
