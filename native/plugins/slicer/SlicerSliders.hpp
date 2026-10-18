/*
 * NULL JSFX - Slicer: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "SlicerDSP.hpp"

#include <cmath>

namespace nullslicer {

static const char* const kPatternOptions[] = { "Straight", "Shuffle", "Reverse Pairs", "Stutter", "Backspin", "Random" };
static const char* const kPatternShort[] = { "STRAIGHT", "SHUFFLE", "PAIRS", "STUTTER", "BACKSPIN", "RANDOM" };
static const char* const kSliceOptions[] = { "8", "16", "32" };

// slider1..slider8 of slicer.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Pattern", "pattern", 0.0f, 5.0f, 1.0f, "", 1.0f, kPatternOptions },
    { "Variation", "variation", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Seed", "seed", 1.0f, 999.0f, 1.0f, "", 1.0f },
    { "Slices", "slices", 0.0f, 2.0f, 1.0f, "", 1.0f, kSliceOptions },
    { "Reverse Chance", "reverse", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Repeat Chance", "repeat", 0.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Gate", "gate", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's upper-case pattern names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Pattern", "pattern", 0.0f, 5.0f, 1.0f, "", 1.0f, kPatternShort },
    kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6], kSliders[7],
};

// the parameter values as DSP settings
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    p.pattern = int(std::lround(value(0)));
    p.variation = std::round(value(1));
    p.seed = std::round(value(2));
    p.slices = int(std::lround(value(3)));
    p.reversePct = std::round(value(4));
    p.repeatPct = std::round(value(5));
    p.gatePct = std::round(value(6));
    p.mixPct = std::round(value(7));
    return p;
}

} // namespace nullslicer
