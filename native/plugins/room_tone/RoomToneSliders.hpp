/*
 * NULL JSFX - Room Tone: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "RoomToneDSP.hpp"

namespace nullrt {

static const char* const kOffOn[] = { "OFF", "ON" };
static const char* const kModeOptions[] = { "GAPS", "BED" };
static const char* const kListenOptions[] = { "OUTPUT", "FILL" };

// slider1..slider7 of room_tone.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Learn", "learn", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Threshold", "threshold", -80.0f, -20.0f, -50.0f, "dB", 1.0f },
    { "Fill Level", "fill_level", -20.0f, 6.0f, 0.0f, "dB", 0.5f },
    { "Fade", "fade", 5.0f, 200.0f, 40.0f, "ms", 1.0f },
    { "Hold", "hold", 0.0f, 500.0f, 80.0f, "ms", 1.0f },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the values as the JSFX sliders hold them (Fill Level: min + k * 0.5 in double)
inline void apply(RoomTone& dsp, const float* v)
{
    const double fill = -20.0 + std::round((double(v[2]) + 20.0) / 0.5) * 0.5;
    dsp.setParams(v[0] >= 0.5f, std::round(v[1]), fill, std::round(v[3]), std::round(v[4]), v[5] >= 0.5f,
                  v[6] >= 0.5f);
}

} // namespace nullrt

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the learnt profile and the fill
class RoomToneBase : public NullMultiPlugin
{
public:
    RoomToneBase() : NullMultiPlugin(nullrt::kSliders, nullrt::kNumSliders) {}
    nullrt::View view;
};

END_NAMESPACE_DISTRHO
