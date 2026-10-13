/*
 * NULL JSFX - Convolution: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * slider2 of the JSFX (the IR File chooser, a file slider) is not a parameter here: the file is
 * the "irfile" state (a path, picked with the interface's file chooser). The parameters are
 * slider1 and slider3..slider8, in order.
 */

#pragma once

#include "NullSliders.hpp"

namespace nullconv {

static const char* const kSourceOptions[] = { "SMALL", "LARGE", "HALL", "PLATE", "CHURCH", "IR FILE" };

static const DISTRHO::SliderInfo kSliders[] = {
    { "Source", "source", 0.0f, 5.0f, 2.0f, "", 1.0f, kSourceOptions },
    { "Length", "length", 10.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Pre-Delay", "predelay", 0.0f, 200.0f, 0.0f, "ms", 1.0f },
    { "Low Cut", "lowcut", 20.0f, 1000.0f, 80.0f, "Hz", 1.0f },
    { "High Cut", "highcut", 1000.0f, 20000.0f, 12000.0f, "Hz", 10.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullconv
