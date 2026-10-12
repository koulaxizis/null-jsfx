/*
 * NULL JSFX - Noise Reduction: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "NoiseReductionDSP.hpp"

namespace nullnr {

static const char* const kOffOn[] = { "OFF", "ON" };
static const char* const kListenOptions[] = { "OUTPUT", "REMOVED" };

// slider1..slider9 of noise_reduction.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Learn", "learn", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Auto", "auto", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Reduction", "reduction", 0.0f, 60.0f, 18.0f, "dB", 1.0f },
    { "Threshold", "threshold", -10.0f, 20.0f, 3.0f, "dB", 0.5f },
    { "Smoothing", "smoothing", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Attack", "attack", 0.0f, 100.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 10.0f, 1000.0f, 150.0f, "ms", 1.0f },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullnr

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the spectrum, the noise
// estimate and the reduction curve
class NoiseReductionBase : public NullMultiPlugin
{
public:
    NoiseReductionBase() : NullMultiPlugin(nullnr::kSliders, nullnr::kNumSliders) {}
    nullnr::View view;
};

END_NAMESPACE_DISTRHO
