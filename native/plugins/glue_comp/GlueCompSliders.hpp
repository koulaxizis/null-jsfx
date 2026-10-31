/*
 * NULL JSFX - Glue Comp: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullgluecomp {

static const char* const kRatioOptions[] = { "2", "4", "10" };
static const char* const kAttackOptions[] = { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" };
static const char* const kReleaseOptions[] = { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" };

// slider1..slider7 of glue_comp.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Threshold", "threshold", -40.0f, 0.0f, -10.0f, "dB", 0.1f },
    { "Ratio", "ratio", 0.0f, 2.0f, 0.0f, "", 1.0f, kRatioOptions },
    { "Attack", "attack", 0.0f, 5.0f, 4.0f, "", 1.0f, kAttackOptions },
    { "Release", "release", 0.0f, 4.0f, 4.0f, "", 1.0f, kReleaseOptions },
    { "Makeup", "makeup", 0.0f, 20.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Sidechain HPF", "sc_hpf", 0.0f, 300.0f, 0.0f, "Hz", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUiRatio[] = { "2:1", "4:1", "10:1" };
static const char* const kUiAttack[] = { "0.1 MS", "0.3 MS", "1 MS", "3 MS", "10 MS", "30 MS" };
static const char* const kUiRelease[] = { "0.1 S", "0.3 S", "0.6 S", "1.2 S", "AUTO" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Ratio", "ratio", 0.0f, 2.0f, 0.0f, "", 1.0f, kUiRatio },
    { "Attack", "attack", 0.0f, 5.0f, 4.0f, "", 1.0f, kUiAttack },
    { "Release", "release", 0.0f, 4.0f, 4.0f, "", 1.0f, kUiRelease },
    kSliders[4], kSliders[5], kSliders[6],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullgluecomp
