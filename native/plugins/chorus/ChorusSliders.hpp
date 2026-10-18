/*
 * NULL JSFX - Chorus: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullchorus {

static const char* const kSyncOptions[] = { "Off", "1/1", "1/2", "1/4", "1/8", "1/16",
                                             "2/1", "4/1", "1/4T", "1/8T", "1/4D", "1/8D" };
static const char* const kSyncShort[] = { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16",
                                          "2/1", "4/1", "1/4T", "1/8T", "1/4D", "1/8D" };
static const char* const kModeOptions[] = { "Classic", "Dimension" };
static const char* const kModeShort[] = { "CLASSIC", "DIM" };

// slider1..slider9 of chorus.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Rate", "rate", 0.1f, 20.0f, 5.0f, "Hz", 0.1f },
    { "Depth", "depth", 0.0f, 10.0f, 3.0f, "ms", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Voices", "voices", 1.0f, 4.0f, 1.0f, "", 1.0f },
    { "Stereo Spread", "spread", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Feedback", "feedback", 0.0f, 90.0f, 0.0f, "%", 1.0f },
    { "Tempo Sync", "sync", 0.0f, 11.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "High Cut", "highcut", 1000.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selectors' upper-case names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5],
    { "Tempo Sync", "sync", 0.0f, 11.0f, 0.0f, "", 1.0f, kSyncShort },
    kSliders[7],
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeShort },
};

// the LFO cycle in beats of Tempo Sync option s (1..11), as ch_cycle in the JSFX
inline double syncCycle(int s)
{
    return s == 1 ? 4.0 : s == 2 ? 2.0 : s == 3 ? 1.0 : s == 4 ? 0.5 : s == 5 ? 0.25 : s == 6 ? 8.0 : s == 7 ? 16.0
         : s == 8 ? 2.0 / 3.0 : s == 9 ? 1.0 / 3.0 : s == 10 ? 1.5 : 0.75;
}

// a parameter value on the JSFX slider's grid in double (min + n * step, at most max), as the
// JSFX sees it: a float such as 0.3f would otherwise detune the LFOs slightly
inline double onGrid(float v, double min, double max, double step)
{
    return std::fmin(max, min + std::round((double(v) - min) / step) * step);
}

} // namespace nullchorus
