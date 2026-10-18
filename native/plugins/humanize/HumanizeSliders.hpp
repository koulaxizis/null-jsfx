/*
 * NULL JSFX - Humanize: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "NullSliders.hpp"

#include <atomic>

namespace nullhumanize {

static const char* const kGrooveOptions[] = { "Straight", "Swing 8ths", "Swing 16ths", "Laid Back", "Push" };

// slider1..slider5 of humanize.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Timing", "timing", 0.0f, 30.0f, 10.0f, "ms", 1.0f },
    { "Velocity", "velocity", 0.0f, 50.0f, 10.0f, "%", 1.0f },
    { "Groove", "groove", 0.0f, 4.0f, 0.0f, "", 1.0f, kGrooveOptions },
    { "Groove Amount", "amount", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Seed", "seed", 1.0f, 9999.0f, 1.0f, "", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the selector's short upper-case names, for the interface (as humanize_gfx.jsfx)
static const char* const kGrooveShort[] = { "NONE", "SWING 8", "SWING 16", "LAZY", "PUSH" };

static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1],
    { "Groove", "groove", 0.0f, 4.0f, 0.0f, "", 1.0f, kGrooveShort },
    kSliders[3], kSliders[4],
};

// what the interface draws (as the display-only code of humanize_gfx.jsfx): the last 16 notes
// (beat in, beat out, velocity in, out; oldest first), how many, the tempo and the latency
struct View {
    static constexpr int kNotes = 16;
    std::atomic<float> in[kNotes] {}, out[kNotes] {}, vel[kNotes] {}, velOut[kNotes] {};
    std::atomic<int> count { 0 };
    std::atomic<float> tempo { 120.0f }, latencyMs { 0.0f };
};

} // namespace nullhumanize

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the notes
class HumanizeBase : public NullMultiPlugin
{
public:
    HumanizeBase() : NullMultiPlugin(nullhumanize::kSliders, nullhumanize::kNumSliders) {}
    nullhumanize::View view;
};

END_NAMESPACE_DISTRHO
