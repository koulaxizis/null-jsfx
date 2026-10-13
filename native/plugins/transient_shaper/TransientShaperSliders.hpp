/*
 * NULL JSFX - Transient Shaper: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulltransient {

// slider1..slider3 of transient_shaper.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Attack", "attack", -20.0f, 20.0f, 0.0f, "dB", 0.1f },
    { "Sustain", "sustain", -20.0f, 20.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulltransient
