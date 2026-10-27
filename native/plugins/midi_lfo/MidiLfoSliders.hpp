/*
 * NULL JSFX - MIDI LFO: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullmidilfo {

static const char* const kShapeOptions[] = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random", "Smooth Random" };
static const char* const kSyncOptions[] = { "Free", "Tempo" };
static const char* const kLengthOptions[] = { "8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32",
                                              "1/2T", "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D" };
static const char* const kDestOptions[] = { "CC", "Pitch Bend", "Channel Pressure" };
static const char* const kRetrigOptions[] = { "Off", "On Note" };

// slider1..slider12 of midi_lfo.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Shape", "shape", 0.0f, 6.0f, 0.0f, "", 1.0f, kShapeOptions },
    { "Sync", "sync", 0.0f, 1.0f, 1.0f, "", 1.0f, kSyncOptions },
    { "Rate", "rate", 0.01f, 20.0f, 1.0f, "Hz", 0.01f },
    { "Length", "length", 0.0f, 15.0f, 5.0f, "", 1.0f, kLengthOptions },
    { "Phase", "phase", 0.0f, 360.0f, 0.0f, "deg", 1.0f },
    { "Destination", "destination", 0.0f, 2.0f, 0.0f, "", 1.0f, kDestOptions },
    { "CC Number", "cc_number", 0.0f, 127.0f, 1.0f, "", 1.0f },
    { "Channel", "channel", 1.0f, 16.0f, 1.0f, "", 1.0f },
    { "Min", "min", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Max", "max", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Retrigger", "retrigger", 0.0f, 1.0f, 0.0f, "", 1.0f, kRetrigOptions },
    { "Seed", "seed", 1.0f, 999.0f, 1.0f, "", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the selectors' short upper-case names, for the interface (as midi_lfo_gfx.jsfx)
static const char* const kShapeShort[] = { "SINE", "TRI", "SAW UP", "SAW DN", "SQUARE", "RANDOM", "SMOOTH" };
static const char* const kSyncShort[] = { "FREE", "TEMPO" };
static const char* const kLengthShort[] = { "8 BARS", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/32",
                                            "1/2T", "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D" };
static const char* const kDestShort[] = { "CC", "BEND", "PRESSURE" };
static const char* const kRetrigShort[] = { "OFF", "NOTE" };

static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Shape", "shape", 0.0f, 6.0f, 0.0f, "", 1.0f, kShapeShort },
    { "Sync", "sync", 0.0f, 1.0f, 1.0f, "", 1.0f, kSyncShort },
    kSliders[2],
    { "Length", "length", 0.0f, 15.0f, 5.0f, "", 1.0f, kLengthShort },
    kSliders[4],
    { "Destination", "destination", 0.0f, 2.0f, 0.0f, "", 1.0f, kDestShort },
    kSliders[6], kSliders[7], kSliders[8], kSliders[9],
    { "Retrigger", "retrigger", 0.0f, 1.0f, 0.0f, "", 1.0f, kRetrigShort },
    kSliders[11],
};

// the Length options in beats
static const double kLengthBeats[16] = { 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                         4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 3.0, 1.5, 0.75 };

} // namespace nullmidilfo
