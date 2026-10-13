/*
 * NULL JSFX - Band Splitter: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullbandsplit {

static const char* const kModeOptions[] = { "SPLIT", "MERGE" };
static const char* const kBandsOptions[] = { "2", "3", "4" };
static const char* const kListenOptions[] = { "OFF", "BAND 1", "BAND 2", "BAND 3", "BAND 4" };

// slider1..slider6 of band_splitter.jsfx, in order (the crossovers are ":log" sliders; the
// interface gives their knobs a log taper)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Bands", "bands", 2.0f, 4.0f, 3.0f, "", 1.0f, kBandsOptions },
    { "Crossover 1", "crossover1", 30.0f, 1000.0f, 150.0f, "Hz", 1.0f },
    { "Crossover 2", "crossover2", 200.0f, 6000.0f, 2000.0f, "Hz", 1.0f },
    { "Crossover 3", "crossover3", 1000.0f, 16000.0f, 7000.0f, "Hz", 1.0f },
    { "Listen", "listen", 0.0f, 4.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullbandsplit
