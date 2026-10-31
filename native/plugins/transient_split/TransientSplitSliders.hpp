/*
 * NULL JSFX - Transient Split: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "TransientSplitDSP.hpp"

namespace nulltransientsplit {

static const char* const kOpt0[] = { "Balance", "Split" };

// slider1..slider6 of transient_split.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt0 },
    { "Transient", "transient", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Tonal", "tonal", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Sensitivity", "sensitivity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 5.0f, 500.0f, 60.0f, "ms", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi0[] = { "BALANCE", "SPLIT" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi0 },
    kSliders[1],
    kSliders[2],
    kSliders[3],
    kSliders[4],
    kSliders[5],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.mode = int(v(0));
    p.transient = v(1);
    p.tonal = v(2);
    p.sensitivity = v(3);
    p.decay = v(4);
    p.output = v(5);
    return p;
}

} // namespace nulltransientsplit
