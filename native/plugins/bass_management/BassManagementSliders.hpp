/*
 * NULL JSFX - Bass Management: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullbm {

static const char* const kBedOptions[] = { "5.1", "7.1", "7.1.4" };
static const char* const kMainsOptions[] = { "SPLIT", "FULL" };
static const char* const kLfeOptions[] = { "0 DB", "+10 DB" };
static const char* const kListenOptions[] = { "ALL", "SUB", "MAINS" };

// slider1..slider7 of bass_management.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Bed", "bed", 0.0f, 2.0f, 2.0f, "", 1.0f, kBedOptions },
    { "Crossover", "crossover", 40.0f, 200.0f, 80.0f, "Hz", 1.0f },
    { "Mains", "mains", 0.0f, 1.0f, 0.0f, "", 1.0f, kMainsOptions },
    { "LFE Gain", "lfe_gain", 0.0f, 1.0f, 1.0f, "", 1.0f, kLfeOptions },
    { "Sub Level", "sub_level", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Listen", "listen", 0.0f, 2.0f, 0.0f, "", 1.0f, kListenOptions },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

static const char* const kPinNames[12] = { "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr" };

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullbm
