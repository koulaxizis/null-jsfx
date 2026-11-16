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
static const char* const kOffOn[] = { "OFF", "ON" };

// slider1..slider25 of volume_shaper.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Shape", "shape", 0.0f, 9.0f, 0.0f, "", 1.0f, kShapeOptions },
    { "Rate", "rate", 0.0f, 6.0f, 2.0f, "", 1.0f, kRateOptions },
    { "Depth", "depth", 0.0f, 100.0f, 80.0f, "%", 1.0f },
    { "Smooth", "smooth", 0.0f, 30.0f, 3.0f, "ms", 1.0f },
    { "Phase", "phase", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Trigger", "trigger", 0.0f, 2.0f, 0.0f, "", 1.0f, kTriggerOptions },
    { "Threshold", "threshold", -60.0f, 0.0f, -20.0f, "dB", 1.0f },
    // 4.0: the drawn shape (appended)
    { "Custom", "custom", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Point 1", "point1", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Point 2", "point2", 0.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Point 3", "point3", 0.0f, 100.0f, 65.0f, "%", 1.0f },
    { "Point 4", "point4", 0.0f, 100.0f, 80.0f, "%", 1.0f },
    { "Point 5", "point5", 0.0f, 100.0f, 90.0f, "%", 1.0f },
    { "Point 6", "point6", 0.0f, 100.0f, 96.0f, "%", 1.0f },
    { "Point 7", "point7", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 8", "point8", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 9", "point9", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 10", "point10", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 11", "point11", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 12", "point12", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 13", "point13", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 14", "point14", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 15", "point15", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Point 16", "point16", 0.0f, 100.0f, 100.0f, "%", 1.0f },
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
    p.custom = value(8) >= 0.5f ? 1 : 0;
    for (int k = 0; k < kPoints; ++k)
        p.points[k] = std::round(value(uint32_t(9 + k)));
    return p;
}

} // namespace nullvolshaper
