/*
 * NULL JSFX - Kick: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "KickDSP.hpp"

namespace nullkick {

static const char* const kOffOn[] = { "Off", "On" };

// slider1..slider10 of kick.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Pitch", "pitch", 30.0f, 120.0f, 50.0f, "Hz", 0.1f },
    { "Follow Notes", "follow_notes", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Sweep", "sweep", 0.0f, 48.0f, 24.0f, "st", 0.1f },
    { "Sweep Time", "sweep_time", 2.0f, 300.0f, 40.0f, "ms", 0.1f },
    { "Decay", "decay", 20.0f, 3000.0f, 450.0f, "ms", 1.0f },
    { "Click", "click", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Click Tone", "click_tone", 500.0f, 12000.0f, 3000.0f, "Hz", 1.0f },
    { "Drive", "drive", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUiOffOn[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Follow Notes", "follow_notes", 0.0f, 1.0f, 0.0f, "", 1.0f, kUiOffOn },
    kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6], kSliders[7], kSliders[8], kSliders[9],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.pitch = v(0);
    p.follow = v(1) >= 0.5;
    p.sweep = v(2);
    p.sweepTime = v(3);
    p.decay = v(4);
    p.click = v(5);
    p.clickTone = v(6);
    p.drive = v(7);
    p.velocity = v(8);
    p.volume = v(9);
    return p;
}

} // namespace nullkick
