/*
 * NULL JSFX - Downmix Check: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulldmx {

static const char* const kBedOptions[] = { "5.1", "7.1", "7.1.4" };
static const char* const kMonitorOptions[] = { "BED", "5.1", "LO/RO", "MONO" };
static const char* const kSoloOptions[] = { "OFF", "L", "R", "C", "LFE", "LS", "RS", "LRS", "RRS", "LTF", "RTF", "LTR", "RTR" };

// slider1..slider8 of downmix_check.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Bed", "bed", 0.0f, 2.0f, 2.0f, "", 1.0f, kBedOptions },
    { "Monitor", "monitor", 0.0f, 3.0f, 2.0f, "", 1.0f, kMonitorOptions },
    { "Center Mix", "center_mix", -6.0f, 0.0f, -3.0f, "dB", 0.5f },
    { "Surround Mix", "surround_mix", -6.0f, 0.0f, -3.0f, "dB", 0.5f },
    { "Height Mix", "height_mix", -6.0f, 0.0f, -3.0f, "dB", 0.5f },
    { "LFE Mix", "lfe_mix", -60.0f, 10.0f, -60.0f, "dB", 0.5f },
    { "Solo", "solo", 0.0f, 12.0f, 0.0f, "", 1.0f, kSoloOptions },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

static const char* const kPinNames[12] = { "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr" };

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nulldmx
