/*
 * NULL JSFX - Phaser: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullphaser {

static const char* const kSyncOptions[] = { "Off", "1/1", "1/2", "1/4", "1/8", "1/16", "2/1", "4/1",
                                             "1/4T", "1/8T", "1/4D", "1/8D" };
static const char* const kSyncShort[] = { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16", "2/1", "4/1",
                                           "1/4T", "1/8T", "1/4D", "1/8D" };

static const char* const kBarberOptions[] = { "Off", "Up", "Down" };
static const char* const kBarberShort[] = { "OFF", "UP", "DOWN" };

// slider1..slider9 of phaser.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Rate", "rate", 0.1f, 5.0f, 0.5f, "Hz", 0.1f },
    { "Stages", "stages", 1.0f, 8.0f, 4.0f, "", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Depth", "depth", 0.0f, 100.0f, 57.0f, "%", 1.0f },
    { "Feedback", "feedback", -95.0f, 95.0f, 30.0f, "%", 1.0f },
    { "Center", "center", 200.0f, 5000.0f, 1020.0f, "Hz", 1.0f },
    { "Stereo Phase", "stereophase", 0.0f, 180.0f, 0.0f, "deg", 1.0f },
    { "Tempo Sync", "sync", 0.0f, 11.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Barber Pole", "barber", 0.0f, 2.0f, 0.0f, "", 1.0f, kBarberOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's upper-case names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6],
    { "Tempo Sync", "sync", 0.0f, 11.0f, 0.0f, "", 1.0f, kSyncShort },
    { "Barber Pole", "barber", 0.0f, 2.0f, 0.0f, "", 1.0f, kBarberShort },
};

// a parameter value on the JSFX slider's grid in double (min + n * step, at most max), as the
// JSFX sees it: a float such as 0.3f would otherwise detune the LFOs slightly
inline double onGrid(float v, double min, double max, double step)
{
    return std::fmin(max, min + std::round((double(v) - min) / step) * step);
}

} // namespace nullphaser

namespace nullphaser {

// the @slider parameters from the plugin's values (in slider order), on the JSFX grid
template <typename DSP, typename V>
inline void applyParams(DSP& dsp, const V& v)
{
    dsp.setParams(onGrid(v(0), 0.1, 5.0, 0.1), v(1), v(2), onGrid(v(3), 0.0, 100.0, 1.0), onGrid(v(4), -95.0, 95.0, 1.0),
                  onGrid(v(5), 200.0, 5000.0, 1.0), onGrid(v(6), 0.0, 180.0, 1.0), int(std::lround(v(7))),
                  int(std::lround(v(8))));
}

} // namespace nullphaser
