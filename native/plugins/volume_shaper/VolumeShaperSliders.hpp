/*
 * NULL JSFX - Volume Shaper: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "VolumeShaperDSP.hpp"

#include <cmath>

namespace nullvolshaper {

static const char* const kShapeOptions[] = { "PUMP", "PUMP HD", "SAW DN", "SAW UP", "GATE",
                                             "TRANCE", "SINE", "TRIANGLE", "HALFTIME", "STUTTER" };
static const char* const kRateOptions[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T" };
static const char* const kTriggerOptions[] = { "TEMPO", "MIDI", "SIDE" };

// slider1..slider8 of volume_shaper.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Shape", "shape", 0.0f, 9.0f, 0.0f, "", 1.0f, kShapeOptions },
    { "Rate", "rate", 0.0f, 6.0f, 2.0f, "", 1.0f, kRateOptions },
    { "Depth", "depth", 0.0f, 100.0f, 80.0f, "%", 1.0f },
    { "Smooth", "smooth", 0.0f, 30.0f, 3.0f, "ms", 1.0f },
    { "Phase", "phase", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Trigger", "trigger", 0.0f, 2.0f, 0.0f, "", 1.0f, kTriggerOptions },
    { "Threshold", "threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the parameter values as DSP settings
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    p.shape = int(std::lround(value(0)));
    p.rate = int(std::lround(value(1)));
    p.depthPct = std::round(value(2));
    p.smoothMs = std::round(value(3));
    p.phasePct = std::round(value(4));
    p.mixPct = std::round(value(5));
    p.trigger = int(std::lround(value(6)));
    p.thresholdDb = std::round(value(7));
    return p;
}

} // namespace nullvolshaper
