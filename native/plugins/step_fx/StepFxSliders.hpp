/*
 * NULL JSFX - Step FX: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "StepFxDSP.hpp"

#include <cmath>

namespace nullstepfx {

static const char* const kEffectOptions[] = { "OFF", "REPEAT", "REVERSE", "FILTER", "GATE", "TAPE STOP", "PITCH DOWN", "PITCH UP", "CRUSH" };
static const char* const kLengthOptions[] = { "1/8", "1/16", "1/32", "1/8T", "1/16T" };
static const char* const kRepeatOptions[] = { "1/16", "1/32", "1/64" };
static const char* const kOnOff[] = { "OFF", "ON" };

// short names for the step grid (step_fx_gfx.jsfx draws the same)
static const char* const kEffectShort[] = { "-", "REPEAT", "REVERSE", "FILTER", "GATE", "TAPE", "PITCH -", "PITCH +", "CRUSH" };

// slider1..slider25 of step_fx.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Step 1", "step1", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 2", "step2", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 3", "step3", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 4", "step4", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 5", "step5", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 6", "step6", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 7", "step7", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 8", "step8", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 9", "step9", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 10", "step10", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 11", "step11", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 12", "step12", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 13", "step13", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 14", "step14", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 15", "step15", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step 16", "step16", 0.0f, 8.0f, 0.0f, "", 1.0f, kEffectOptions },
    { "Step Length", "step_length", 0.0f, 4.0f, 1.0f, "", 1.0f, kLengthOptions },
    { "Steps", "steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Swing", "swing", 0.0f, 75.0f, 0.0f, "%", 1.0f },
    { "Repeat Size", "repeat_size", 0.0f, 2.0f, 1.0f, "", 1.0f, kRepeatOptions },
    { "Filter", "filter", 200.0f, 8000.0f, 800.0f, "Hz", 1.0f },
    { "Crush", "crush", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Pitch", "pitch", 1.0f, 12.0f, 12.0f, "semitones", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "MIDI Trigger", "midi_trigger", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the parameter values as DSP settings (every slider has step 1: the nearest integer)
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    for (int k = 0; k < 16; ++k)
        p.steps[k] = int(std::lround(std::fmax(0.0f, std::fmin(8.0f, value(uint32_t(k))))));
    p.stepLength = int(std::lround(value(16)));
    p.numSteps = int(std::lround(value(17)));
    p.swing = std::round(double(value(18)));
    p.repeatSize = int(std::lround(value(19)));
    p.filterHz = std::round(double(value(20)));
    p.crush = std::round(double(value(21)));
    p.pitch = std::round(double(value(22)));
    p.mix = std::round(double(value(23)));
    p.midi = value(24) >= 0.5f;
    return p;
}

} // namespace nullstepfx
