/*
 * NULL JSFX - Arpeggiator: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullarp {

static const char* const kRateOptions[] = { "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/16D", "1/16T", "1/32" };
static const char* const kPatternOptions[] = { "Up", "Down", "Up-Down", "Down-Up", "Played", "Random", "Chord" };
static const char* const kLatchOptions[] = { "Off", "On" };

// slider1..slider24 of arpeggiator.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Rate", "rate", 0.0f, 9.0f, 6.0f, "", 1.0f, kRateOptions },
    { "Pattern", "pattern", 0.0f, 6.0f, 0.0f, "", 1.0f, kPatternOptions },
    { "Octaves", "octaves", 1.0f, 4.0f, 1.0f, "", 1.0f },
    { "Gate", "gate", 5.0f, 200.0f, 50.0f, "%", 1.0f },
    { "Swing", "swing", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Latch", "latch", 0.0f, 1.0f, 0.0f, "", 1.0f, kLatchOptions },
    { "Steps", "steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Seed", "seed", 1.0f, 9999.0f, 1.0f, "", 1.0f },
    { "Step 1", "step1", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 2", "step2", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 3", "step3", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 4", "step4", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 5", "step5", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 6", "step6", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 7", "step7", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 8", "step8", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 9", "step9", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 10", "step10", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 11", "step11", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 12", "step12", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 13", "step13", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 14", "step14", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 15", "step15", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Step 16", "step16", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the selectors' short upper-case names, for the interface (as arpeggiator_gfx.jsfx)
static const char* const kPatternShort[] = { "UP", "DOWN", "UP-DN", "DN-UP", "PLAYED", "RANDOM", "CHORD" };
static const char* const kLatchShort[] = { "OFF", "ON" };

static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Pattern", "pattern", 0.0f, 6.0f, 0.0f, "", 1.0f, kPatternShort },
    kSliders[2], kSliders[3], kSliders[4],
    { "Latch", "latch", 0.0f, 1.0f, 0.0f, "", 1.0f, kLatchShort },
    kSliders[6], kSliders[7],
    kSliders[8], kSliders[9], kSliders[10], kSliders[11], kSliders[12], kSliders[13], kSliders[14], kSliders[15],
    kSliders[16], kSliders[17], kSliders[18], kSliders[19], kSliders[20], kSliders[21], kSliders[22], kSliders[23],
};

} // namespace nullarp
