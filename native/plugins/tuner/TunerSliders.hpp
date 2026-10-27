/*
 * NULL JSFX - Tuner: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "TunerDSP.hpp"

namespace nulltuner {

static const char* const kDisplayOptions[] = { "NEEDLE", "STROBE" };
static const char* const kInputOptions[] = { "STEREO", "LEFT", "RIGHT" };
static const char* const kOutputOptions[] = { "THRU", "MUTE" };

// slider1..slider4 of tuner.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Reference A4", "reference_a4", 415.0f, 466.0f, 440.0f, "Hz", 1.0f },
    { "Display", "display", 0.0f, 1.0f, 0.0f, "", 1.0f, kDisplayOptions },
    { "Input", "input", 0.0f, 2.0f, 0.0f, "", 1.0f, kInputOptions },
    { "Output", "output", 0.0f, 1.0f, 0.0f, "", 1.0f, kOutputOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulltuner

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the note
class TunerBase : public NullMultiPlugin
{
public:
    TunerBase() : NullMultiPlugin(nulltuner::kSliders, nulltuner::kNumSliders) {}
    nulltuner::View view;
};

END_NAMESPACE_DISTRHO
