/*
 * NULL JSFX - Octaver: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

#include <atomic>
#include <cmath>

namespace nulloctaver {

// slider1..slider6 of octaver.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Dry", "dry", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Oct -1", "oct_1", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Oct -2", "oct_2", 0.0f, 100.0f, 35.0f, "%", 1.0f },
    { "Up", "up", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Tone", "tone", 100.0f, 5000.0f, 900.0f, "Hz", 1.0f },
    { "Tracking", "tracking", 60.0f, 1000.0f, 300.0f, "Hz", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// what the interface draws (as the display-only code of octaver_gfx.jsfx): peak levels in
// linear amplitude and the tracked input frequency (0 = none)
struct View {
    std::atomic<float> in { 0.0f }, oct1 { 0.0f }, oct2 { 0.0f }, up { 0.0f }, out { 0.0f }, hz { 0.0f };
};

} // namespace nulloctaver

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the meters
class OctaverBase : public NullMultiPlugin
{
public:
    OctaverBase() : NullMultiPlugin(nulloctaver::kSliders, nulloctaver::kNumSliders) {}
    nulloctaver::View view;
};

END_NAMESPACE_DISTRHO
