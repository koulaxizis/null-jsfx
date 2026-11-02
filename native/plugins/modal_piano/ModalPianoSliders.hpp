/*
 * NULL JSFX - Modal Piano: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullpiano {

// slider1..slider10 of modal_piano.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Hardness", "hardness", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 25.0f, 400.0f, 100.0f, "%", 1.0f },
    { "Unison", "unison", 0.0f, 4.0f, 1.2f, "cents", 0.1f },
    { "Inharmonicity", "inharmonicity", 0.0f, 200.0f, 100.0f, "%", 1.0f },
    { "Hammer Noise", "hammer_noise", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Damper", "damper", 30.0f, 1000.0f, 250.0f, "ms", 1.0f },
    { "Body", "body", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Width", "width", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, -6.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullpiano
