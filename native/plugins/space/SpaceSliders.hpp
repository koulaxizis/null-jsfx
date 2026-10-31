/*
 * NULL JSFX - Space: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "SpaceDSP.hpp"

namespace nullspace {

static const char* const kTypeOptions[] = { "Room", "Chamber", "Hall", "Galaxy" };

// slider1..slider7 of space.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Type", "type", 0.0f, 3.0f, 2.0f, "", 1.0f, kTypeOptions },
    { "Size", "size", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Damping", "damping", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Detune", "detune", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Pre-Delay", "pre_delay", 0.0f, 250.0f, 0.0f, "ms", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

static const char* const kUiType[] = { "ROOM", "CHAMBER", "HALL", "GALAXY" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Type", "type", 0.0f, 3.0f, 2.0f, "", 1.0f, kUiType },
    kSliders[1], kSliders[2], kSliders[3], kSliders[4], kSliders[5], kSliders[6],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

template <typename Get>
inline Params paramsFrom(Get v)
{
    Params p;
    p.type = int(v(0));
    p.size = v(1);
    p.decay = v(2);
    p.damping = v(3);
    p.detune = v(4);
    p.preDelay = v(5);
    p.mix = v(6);
    return p;
}

} // namespace nullspace
