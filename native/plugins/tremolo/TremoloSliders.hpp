/*
 * NULL JSFX - Tremolo: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nulltremolo {

static const char* const kShapeOptions[] = { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" };
static const char* const kSyncOptions[] = { "Off", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4D", "1/8D" };
static const char* const kModeOptions[] = { "Classic", "Harmonic" };
static const char* const kShapeShort[] = { "SINE", "TRI", "SQUARE", "SAW UP", "SAW DN" };
static const char* const kSyncShort[] = { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4D", "1/8D" };
static const char* const kModeShort[] = { "CLASSIC", "HARM" };

// slider1..slider7 of tremolo.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Rate", "rate", 0.1f, 20.0f, 5.0f, "Hz", 0.1f },
    { "Depth", "depth", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Shape", "shape", 0.0f, 4.0f, 0.0f, "", 1.0f, kShapeOptions },
    { "Tempo Sync", "sync", 0.0f, 11.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Stereo Phase", "stereo_phase", 0.0f, 180.0f, 0.0f, "deg", 1.0f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selectors' upper-case names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2],
    { "Shape", "shape", 0.0f, 4.0f, 0.0f, "", 1.0f, kShapeShort },
    { "Tempo Sync", "sync", 0.0f, 11.0f, 0.0f, "", 1.0f, kSyncShort },
    kSliders[5],
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeShort },
};

// a parameter value on the JSFX slider's grid in double (min + n * step, at most max), as the
// JSFX sees it: a float such as 0.3f would otherwise detune the LFOs slightly
inline double onGrid(float v, double min, double max, double step)
{
    return std::fmin(max, min + std::round((double(v) - min) / step) * step);
}

} // namespace nulltremolo
