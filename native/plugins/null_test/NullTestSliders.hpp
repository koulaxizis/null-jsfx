/*
 * NULL JSFX - Null Test: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "NullTestDSP.hpp"

namespace nullnulltest {

static const char* const kOpt0[] = { "Main", "Sidechain", "Difference" };
static const char* const kOpt1[] = { "Manual", "Auto" };
static const char* const kOpt5[] = { "Normal", "Inverted" };

// slider1..slider6 of null_test.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Listen", "listen", 0.0f, 2.0f, 2.0f, "", 1.0f, kOpt0 },
    { "Align", "align", 0.0f, 1.0f, 1.0f, "", 1.0f, kOpt1 },
    { "Range", "range", 1.0f, 200.0f, 20.0f, "ms", 1.0f },
    { "Offset", "offset", -10000.0f, 10000.0f, 0.0f, "samples", 1.0f },
    { "Trim", "trim", -24.0f, 24.0f, 0.0f, "dB", 0.01f },
    { "Polarity", "polarity", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt5 },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi0[] = { "MAIN", "SIDECHAIN", "DIFFERENCE" };
static const char* const kUi1[] = { "MANUAL", "AUTO" };
static const char* const kUi5[] = { "NORMAL", "INVERTED" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Listen", "listen", 0.0f, 2.0f, 2.0f, "", 1.0f, kUi0 },
    { "Align", "align", 0.0f, 1.0f, 1.0f, "", 1.0f, kUi1 },
    kSliders[2],
    kSliders[3],
    kSliders[4],
    { "Polarity", "polarity", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi5 },
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.listen = int(v(0));
    p.align = int(v(1));
    p.range = v(2);
    p.offset = v(3);
    p.trim = v(4);
    p.polarity = int(v(5));
    return p;
}

} // namespace nullnulltest
