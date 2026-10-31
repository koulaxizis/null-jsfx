/*
 * NULL JSFX - Pluck: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullpluck {

static const char* const kModelOptions[] = { "Nylon Guitar", "Steel Guitar", "Harp", "Koto", "Pipa", "Banjo", "Muted Guitar", "Dulcimer" };
static const char* const kModelShort[] = { "NYLON", "STEEL", "HARP", "KOTO", "PIPA", "BANJO", "MUTED", "DULCIMER" };
static const char* const kStrumOptions[] = { "Off", "Down", "Up", "Down-Up", "Folk", "Offbeat" };
static const char* const kStrumShort[] = { "OFF", "DOWN", "UP", "DOWN-UP", "FOLK", "OFFBEAT" };
static const char* const kRateOptions[] = { "Quarters", "Eighths", "Sixteenths", "Eighth Triplets" };
static const char* const kRateShort[] = { "1/4", "1/8", "1/16", "1/8 T" };
static const char* const kPizzOptions[] = { "Off", "Violins", "Cellos" };
static const char* const kPizzShort[] = { "OFF", "VIOLINS", "CELLOS" };

// slider1..slider13 of pluck.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Model", "model", 0.0f, 7.0f, 0.0f, "", 1.0f, kModelOptions },
    { "Pick Position", "pick_position", 5.0f, 50.0f, 18.0f, "%", 1.0f },
    { "Brightness", "brightness", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 10.0f, 300.0f, 100.0f, "%", 1.0f },
    { "Release", "release", 10.0f, 2000.0f, 150.0f, "ms", 1.0f },
    { "Body", "body", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Width", "width", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Strum", "strum", 0.0f, 5.0f, 0.0f, "", 1.0f, kStrumOptions },
    { "Strum Rate", "strum_rate", 0.0f, 3.0f, 1.0f, "", 1.0f, kRateOptions },
    { "Strum Spread", "strum_spread", 0.0f, 100.0f, 20.0f, "ms", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Pizzicato", "pizzicato", 0.0f, 2.0f, 0.0f, "", 1.0f, kPizzOptions },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullpluck
