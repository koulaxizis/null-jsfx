/*
 * NULL JSFX - Pitch to MIDI: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "PitchToMidiDSP.hpp"

namespace nullpitchtomidi {

static const char* const kOpt4[] = { "Fixed", "Level" };
static const char* const kOpt7[] = { "Off", "On" };
static const char* const kOpt8[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };

// slider1..slider9 of pitch_to_midi.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", -70.0f, 0.0f, -40.0f, "dB", 0.1f },
    { "Low Note", "low_note", 28.0f, 127.0f, 36.0f, "", 1.0f },
    { "High Note", "high_note", 28.0f, 127.0f, 84.0f, "", 1.0f },
    { "Stability", "stability", 10.0f, 200.0f, 40.0f, "ms", 1.0f },
    { "Velocity", "velocity", 0.0f, 1.0f, 1.0f, "", 1.0f, kOpt4 },
    { "Fixed Velocity", "fixed_velocity", 1.0f, 127.0f, 100.0f, "", 1.0f },
    { "Transpose", "transpose", -24.0f, 24.0f, 0.0f, "semitones", 1.0f },
    { "Pitch Bend", "pitch_bend", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt7 },
    { "Channel", "channel", 0.0f, 15.0f, 0.0f, "", 1.0f, kOpt8 },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi4[] = { "FIXED", "LEVEL" };
static const char* const kUi7[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    kSliders[1],
    kSliders[2],
    kSliders[3],
    { "Velocity", "velocity", 0.0f, 1.0f, 1.0f, "", 1.0f, kUi4 },
    kSliders[5],
    kSliders[6],
    { "Pitch Bend", "pitch_bend", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi7 },
    kSliders[8],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.threshold = v(0);
    p.lowNote = v(1);
    p.highNote = v(2);
    p.stability = v(3);
    p.velocityMode = int(v(4));
    p.fixedVelocity = v(5);
    p.transpose = v(6);
    p.pitchBend = int(v(7));
    p.channel = int(v(8));
    return p;
}

} // namespace nullpitchtomidi
