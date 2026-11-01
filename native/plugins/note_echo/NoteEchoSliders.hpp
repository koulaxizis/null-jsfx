/*
 * NULL JSFX - Note Echo: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "NoteEchoDSP.hpp"

namespace nullnoteecho {

static const char* const kOpt2[] = { "Off", "On" };
static const char* const kOpt3[] = { "1/4", "1/8", "1/16", "1/32", "1/2", "1/8T", "1/16T", "1/8D" };
static const char* const kOpt7[] = { "Off", "On" };

// slider1..slider8 of note_echo.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Repeats", "repeats", 1.0f, 16.0f, 3.0f, "", 1.0f },
    { "Time", "time", 10.0f, 2000.0f, 250.0f, "ms", 1.0f },
    { "Tempo Sync", "tempo_sync", 0.0f, 1.0f, 1.0f, "", 1.0f, kOpt2 },
    { "Subdivision", "subdivision", 0.0f, 7.0f, 1.0f, "", 1.0f, kOpt3 },
    { "Velocity Decay", "velocity_decay", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Pitch Step", "pitch_step", -12.0f, 12.0f, 0.0f, "semitones", 1.0f },
    { "Gate", "gate", 5.0f, 95.0f, 50.0f, "%", 1.0f },
    { "Dry", "dry", 0.0f, 1.0f, 1.0f, "", 1.0f, kOpt7 },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi2[] = { "OFF", "ON" };
static const char* const kUi7[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    kSliders[1],
    { "Tempo Sync", "tempo_sync", 0.0f, 1.0f, 1.0f, "", 1.0f, kUi2 },
    kSliders[3],
    kSliders[4],
    kSliders[5],
    kSliders[6],
    { "Dry", "dry", 0.0f, 1.0f, 1.0f, "", 1.0f, kUi7 },
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.repeats = int(v(0));
    p.time = v(1);
    p.sync = int(v(2));
    p.subdivision = int(v(3));
    p.decay = v(4);
    p.pitchStep = v(5);
    p.gate = v(6);
    p.dry = int(v(7));
    return p;
}

} // namespace nullnoteecho
