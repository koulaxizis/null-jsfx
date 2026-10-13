/*
 * NULL JSFX - Loudness Meter: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "LoudnessMeterDSP.hpp"

namespace nullloud {

static const char* const kTargetOptions[] = { "-14 LUFS", "-16 LUFS", "-23 LUFS" };
static const char* const kScaleOptions[] = { "EBU +9", "EBU +18" };
static const char* const kHistoryOptions[] = { "10 S", "30 S", "1 MIN", "5 MIN" };
static const char* const kAutoOptions[] = { "OFF", "ON PLAY" };
static const char* const kResetOptions[] = { "RESET", "RESET" };
static const char* const kMonitorOptions[] = { "STEREO", "MONO" };

// slider1..slider6 of loudness_meter.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Target", "target", 0.0f, 2.0f, 0.0f, "", 1.0f, kTargetOptions },
    { "Scale", "scale", 0.0f, 1.0f, 0.0f, "", 1.0f, kScaleOptions },
    { "History", "history", 0.0f, 3.0f, 1.0f, "", 1.0f, kHistoryOptions },
    { "Auto Reset", "auto_reset", 0.0f, 1.0f, 0.0f, "", 1.0f, kAutoOptions },
    { "Reset", "reset", 0.0f, 1.0f, 0.0f, "", 1.0f, kResetOptions },
    { "Monitor", "monitor", 0.0f, 1.0f, 0.0f, "", 1.0f, kMonitorOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

inline int targetLufs(float v) { const int i = int(std::lround(v)); return i == 0 ? -14 : i == 1 ? -16 : -23; }

} // namespace nullloud

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the measurements
class LoudnessMeterBase : public NullMultiPlugin
{
public:
    LoudnessMeterBase() : NullMultiPlugin(nullloud::kSliders, nullloud::kNumSliders) {}
    nullloud::View view;
};

END_NAMESPACE_DISTRHO
