/*
 * NULL JSFX - Multichannel Loudness: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "MultichannelLoudnessDSP.hpp"

namespace nullmloud {

static const char* const kBedOptions[] = { "5.1", "7.1", "7.1.4" };
static const char* const kTargetOptions[] = { "-23 LUFS", "-24 LUFS", "-18 LUFS", "-14 LUFS" };
static const char* const kHistoryOptions[] = { "10 S", "30 S", "1 MIN", "5 MIN" };
static const char* const kAutoOptions[] = { "OFF", "ON PLAY" };
static const char* const kResetOptions[] = { "RESET", "RESET" };

// slider1..slider5 of multichannel_loudness.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Bed", "bed", 0.0f, 2.0f, 2.0f, "", 1.0f, kBedOptions },
    { "Target", "target", 0.0f, 3.0f, 0.0f, "", 1.0f, kTargetOptions },
    { "History", "history", 0.0f, 3.0f, 1.0f, "", 1.0f, kHistoryOptions },
    { "Auto Reset", "auto_reset", 0.0f, 1.0f, 0.0f, "", 1.0f, kAutoOptions },
    { "Reset", "reset", 0.0f, 1.0f, 0.0f, "", 1.0f, kResetOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

inline int targetLufs(float v) { const int i = int(std::lround(v)); return i == 0 ? -23 : i == 1 ? -24 : i == 2 ? -18 : -14; }

} // namespace nullmloud

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the measurements
class MultichannelLoudnessBase : public NullMultiPlugin
{
public:
    MultichannelLoudnessBase() : NullMultiPlugin(nullmloud::kSliders, nullmloud::kNumSliders) {}
    nullmloud::View view;
};

END_NAMESPACE_DISTRHO
