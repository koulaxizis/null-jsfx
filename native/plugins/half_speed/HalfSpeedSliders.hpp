/*
 * NULL JSFX - Half Speed: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "HalfSpeedDSP.hpp"

#include <atomic>
#include <cmath>

namespace nullhalfspeed {

static const char* const kLengthOptions[] = { "1/4", "1/2", "1 Bar", "2 Bars" };
static const char* const kLengthShort[] = { "1/4", "1/2", "1 BAR", "2 BARS" };
static const char* const kSpeedOptions[] = { "1/2", "1/4", "2/3" };
static const char* const kKeepOptions[] = { "Off", "On" };
static const char* const kKeepShort[] = { "OFF", "ON" };

// slider1..slider6 of half_speed.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Length", "length", 0.0f, 3.0f, 2.0f, "", 1.0f, kLengthOptions },
    { "Speed", "speed", 0.0f, 2.0f, 0.0f, "", 1.0f, kSpeedOptions },
    { "Pitch Keep", "pitch_keep", 0.0f, 1.0f, 0.0f, "", 1.0f, kKeepOptions },
    { "Tone", "tone", 1000.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Sync Offset", "sync_offset", 0.0f, 3.0f, 0.0f, "beats", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selectors' upper-case names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Length", "length", 0.0f, 3.0f, 2.0f, "", 1.0f, kLengthShort },
    kSliders[1],
    { "Pitch Keep", "pitch_keep", 0.0f, 1.0f, 0.0f, "", 1.0f, kKeepShort },
    kSliders[3], kSliders[4], kSliders[5],
};

// the parameter values as DSP settings
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    p.length = int(std::lround(value(0)));
    p.speed = int(std::lround(value(1)));
    p.keep = value(2) >= 0.5f;
    p.toneHz = std::round(value(3));
    p.mixPct = std::round(value(4));
    p.offset = std::round(value(5));
    return p;
}

// what the interface draws (as the display of half_speed_gfx.jsfx): the write and read heads
// as fractions of the captured Length, the cycle position, and the waveform's peaks
struct View {
    std::atomic<float> write { 0.0f }, read { 0.0f }, cycle { 0.0f }, capture { 0.5f };
    std::atomic<float> peaks[HalfSpeedProcessor::kBins] = {};
};

} // namespace nullhalfspeed

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the buffer
class HalfSpeedBase : public NullMultiPlugin
{
public:
    HalfSpeedBase() : NullMultiPlugin(nullhalfspeed::kSliders, nullhalfspeed::kNumSliders) {}
    nullhalfspeed::View view;
};

END_NAMESPACE_DISTRHO
