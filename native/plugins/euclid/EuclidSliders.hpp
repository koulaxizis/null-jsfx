/*
 * NULL JSFX - Euclid: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "EuclidDSP.hpp"

namespace nulleuclid {

static const char* const kOpt3[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
static const char* const kOpt9[] = { "Transport", "Always" };
static const char* const kOpt10[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };

// slider1..slider11 of euclid.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Steps", "steps", 1.0f, 32.0f, 16.0f, "", 1.0f },
    { "Pulses", "pulses", 0.0f, 32.0f, 4.0f, "", 1.0f },
    { "Rotate", "rotate", 0.0f, 31.0f, 0.0f, "", 1.0f },
    { "Rate", "rate", 0.0f, 5.0f, 2.0f, "", 1.0f, kOpt3 },
    { "Note", "note", 0.0f, 127.0f, 36.0f, "", 1.0f },
    { "Velocity", "velocity", 1.0f, 127.0f, 100.0f, "", 1.0f },
    { "Accent", "accent", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Gate", "gate", 5.0f, 95.0f, 50.0f, "%", 1.0f },
    { "Swing", "swing", 0.0f, 75.0f, 0.0f, "%", 1.0f },
    { "Run", "run", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt9 },
    { "Channel", "channel", 0.0f, 15.0f, 9.0f, "", 1.0f, kOpt10 },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi3[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
static const char* const kUi9[] = { "HOST", "ALWAYS" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    kSliders[1],
    kSliders[2],
    { "Rate", "rate", 0.0f, 5.0f, 2.0f, "", 1.0f, kUi3 },
    kSliders[4],
    kSliders[5],
    kSliders[6],
    kSliders[7],
    kSliders[8],
    { "Run", "run", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi9 },
    kSliders[10],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.steps = int(v(0));
    p.pulses = int(v(1));
    p.rotate = int(v(2));
    p.rate = int(v(3));
    p.note = int(v(4));
    p.velocity = int(v(5));
    p.accent = v(6);
    p.gate = v(7);
    p.swing = v(8);
    p.run = int(v(9));
    p.channel = int(v(10));
    return p;
}

} // namespace nulleuclid
