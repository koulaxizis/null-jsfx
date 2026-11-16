/*
 * NULL JSFX - Strum: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "StrumDSP.hpp"

namespace nullstrum {

static const char* const kOpt0[] = { "Down", "Up", "Alternate", "Random" };
static const char* const kOpt2[] = { "Once", "1/4", "1/8", "1/16", "Down-Up 1/8", "Folk", "Offbeat" };

// slider1..slider8 of strum.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Direction", "direction", 0.0f, 3.0f, 0.0f, "", 1.0f, kOpt0 },
    { "Speed", "speed", 0.0f, 100.0f, 12.0f, "ms", 1.0f },
    { "Rhythm", "rhythm", 0.0f, 6.0f, 0.0f, "", 1.0f, kOpt2 },
    { "Gate", "gate", 10.0f, 100.0f, 85.0f, "%", 1.0f },
    { "Velocity Spread", "velocity_spread", 0.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Upstroke Velocity", "upstroke_velocity", 10.0f, 100.0f, 80.0f, "%", 1.0f },
    { "Humanize", "humanize", 0.0f, 20.0f, 0.0f, "ms", 1.0f },
    { "Collect", "collect", 0.0f, 100.0f, 25.0f, "ms", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi0[] = { "DOWN", "UP", "ALTERNATE", "RANDOM" };
static const char* const kUi2[] = { "ONCE", "1/4", "1/8", "1/16", "DOWN-UP", "FOLK", "OFFBEAT" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Direction", "direction", 0.0f, 3.0f, 0.0f, "", 1.0f, kUi0 },
    kSliders[1],
    { "Rhythm", "rhythm", 0.0f, 6.0f, 0.0f, "", 1.0f, kUi2 },
    kSliders[3],
    kSliders[4],
    kSliders[5],
    kSliders[6],
    kSliders[7],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.direction = int(v(0));
    p.speed = v(1);
    p.rhythm = int(v(2));
    p.gate = v(3);
    p.spread = v(4);
    p.upstroke = v(5);
    p.humanize = v(6);
    p.collect = v(7);
    return p;
}

} // namespace nullstrum
