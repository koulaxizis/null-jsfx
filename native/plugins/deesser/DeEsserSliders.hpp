/*
 * NULL JSFX - DeEsser: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nulldeesser {

// slider1..slider2 of deesser.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Target", "target", 4000.0f, 10000.0f, 6000.0f, "Hz", 100.0f },
    { "Reduction", "reduction", -18.0f, 0.0f, -6.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulldeesser
