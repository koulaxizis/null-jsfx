/*
 * NULL JSFX - Reverb: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullreverb {

static const char* const kBypassOptions[] = { "OFF", "ON" };
static const char* const kFreezeOptions[] = { "Off", "On" };
static const char* const kCharOptions[] = { "Classic", "Plate", "Hall" };
static const char* const kFreezeShort[] = { "OFF", "ON" };
static const char* const kCharShort[] = { "CLASSIC", "PLATE", "HALL" };

// slider1..slider12 of reverb.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Room Size", "room_size", 10.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Damping", "damping", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Bypass", "bypass", 0.0f, 1.0f, 0.0f, "", 1.0f, kBypassOptions },
    { "Pre-Delay", "predelay", 0.0f, 250.0f, 0.0f, "ms", 1.0f },
    { "Low Cut", "lowcut", 20.0f, 1000.0f, 20.0f, "Hz", 1.0f },
    { "High Cut", "highcut", 1000.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Width", "width", 0.0f, 200.0f, 100.0f, "%", 1.0f },
    { "Modulation", "modulation", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeOptions },
    { "Character", "character", 0.0f, 2.0f, 0.0f, "", 1.0f, kCharOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selectors' upper-case names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5],
    kSliders[6], kSliders[7], kSliders[8], kSliders[9],
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeShort },
    { "Character", "character", 0.0f, 2.0f, 0.0f, "", 1.0f, kCharShort },
};

} // namespace nullreverb
