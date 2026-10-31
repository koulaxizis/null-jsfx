/*
 * NULL JSFX - Multi-Tap Delay: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "MultiTapDSP.hpp"

namespace nullmultitap {

static const char* const kOpt1[] = { "Even", "Dotted", "Triplet", "Swing", "Speed Up", "Slow Down", "Scatter" };
static const char* const kOpt3[] = { "Off", "On" };
static const char* const kOpt4[] = { "1/4", "1/8", "1/16", "1/32", "1/2" };

// slider1..slider11 of multi_tap_delay.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Taps", "taps", 1.0f, 8.0f, 4.0f, "", 1.0f },
    { "Pattern", "pattern", 0.0f, 6.0f, 0.0f, "", 1.0f, kOpt1 },
    { "Time", "time", 10.0f, 1000.0f, 250.0f, "ms", 1.0f },
    { "Tempo Sync", "tempo_sync", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt3 },
    { "Subdivision", "subdivision", 0.0f, 4.0f, 1.0f, "", 1.0f, kOpt4 },
    { "Decay", "decay", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Spread", "spread", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Feedback", "feedback", 0.0f, 90.0f, 0.0f, "%", 1.0f },
    { "Low Cut", "low_cut", 20.0f, 2000.0f, 20.0f, "Hz", 1.0f },
    { "High Cut", "high_cut", 1000.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 35.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi1[] = { "EVEN", "DOTTED", "TRIPLET", "SWING", "SPEED UP", "SLOW DOWN", "SCATTER" };
static const char* const kUi3[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Pattern", "pattern", 0.0f, 6.0f, 0.0f, "", 1.0f, kUi1 },
    kSliders[2],
    { "Tempo Sync", "tempo_sync", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi3 },
    kSliders[4],
    kSliders[5],
    kSliders[6],
    kSliders[7],
    kSliders[8],
    kSliders[9],
    kSliders[10],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.taps = int(v(0));
    p.pattern = int(v(1));
    p.time = v(2);
    p.sync = v(3) >= 0.5;
    p.subdivision = int(v(4));
    p.decay = v(5);
    p.spread = v(6);
    p.feedback = v(7);
    p.lowCut = v(8);
    p.highCut = v(9);
    p.mix = v(10);
    return p;
}

} // namespace nullmultitap
