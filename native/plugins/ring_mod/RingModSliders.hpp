/*
 * NULL JSFX - Ring Mod: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

#include <cmath>

namespace nullringmod {

// slider1..slider4 of ring_mod.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Rate", "rate", 0.1f, 20.0f, 5.0f, "Hz", 0.1f },
    { "Carrier Freq", "carrier", 100.0f, 2000.0f, 200.0f, "Hz", 10.0f },
    { "Depth", "depth", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 40.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a parameter value on the JSFX slider's grid in double (min + n * step, at most max), as the
// JSFX sees it: a float such as 0.3f would otherwise detune the LFOs slightly
inline double onGrid(float v, double min, double max, double step)
{
    return std::fmin(max, min + std::round((double(v) - min) / step) * step);
}

} // namespace nullringmod
