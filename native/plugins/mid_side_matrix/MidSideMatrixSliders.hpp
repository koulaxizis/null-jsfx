/*
 * NULL JSFX - Mid/Side Matrix: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullmidside {

static const char* const kModeOptions[] = { "ENCODE", "DECODE" };

// slider1..slider3 of mid_side_matrix.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Mid Gain", "mid_gain", -6.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Side Gain", "side_gain", -6.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Mode", "mode", 0.0f, 1.0f, 1.0f, "", 1.0f, kModeOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a 0.1-step slider value as the JSFX sees it (the parameter arrives as a float)
inline double tenth(float v) noexcept { return std::round(double(v) * 10.0) / 10.0; }

} // namespace nullmidside
