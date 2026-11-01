/*
 * NULL JSFX - Test Signal: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "TestSignalDSP.hpp"

namespace nulltestsignal {

static const char* const kOpt0[] = { "Sine", "Sweep", "Pink Noise", "White Noise", "Polarity" };
static const char* const kOpt4[] = { "Both", "Left", "Right" };
static const char* const kOffOn[] = { "Off", "On" };

// slider1..slider7 of test_signal.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Signal", "signal", 0.0f, 4.0f, 0.0f, "", 1.0f, kOpt0 },
    { "Frequency", "frequency", 20.0f, 20000.0f, 1000.0f, "Hz", 1.0f },
    { "Level", "level", -60.0f, 0.0f, -20.0f, "dBFS", 0.1f },
    { "Sweep Time", "sweep_time", 1.0f, 60.0f, 10.0f, "s", 0.1f },
    { "Channels", "channels", 0.0f, 2.0f, 0.0f, "", 1.0f, kOpt4 },
    { "Invert Right", "invert_right", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Pass Input", "pass_input", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi0[] = { "SINE", "SWEEP", "PINK", "WHITE", "POLARITY" };
static const char* const kUi4[] = { "BOTH", "LEFT", "RIGHT" };
static const char* const kUiOffOn[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Signal", "signal", 0.0f, 4.0f, 0.0f, "", 1.0f, kUi0 },
    kSliders[1],
    kSliders[2],
    kSliders[3],
    { "Channels", "channels", 0.0f, 2.0f, 0.0f, "", 1.0f, kUi4 },
    { "Invert Right", "invert_right", 0.0f, 1.0f, 0.0f, "", 1.0f, kUiOffOn },
    { "Pass Input", "pass_input", 0.0f, 1.0f, 0.0f, "", 1.0f, kUiOffOn },
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters (the JSFX's @slider: floor(x + 0.5) and >= 0.5)
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.signal = int(std::floor(v(0) + 0.5));
    p.frequency = v(1);
    p.level = v(2);
    p.sweepTime = v(3);
    p.channels = int(std::floor(v(4) + 0.5));
    p.invert = v(5) >= 0.5;
    p.pass = v(6) >= 0.5;
    return p;
}

} // namespace nulltestsignal
