/*
 * NULL JSFX - Delay: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nulldelay {

static const char* const kSyncOptions[] = { "OFF", "ON" };
static const char* const kNoteOptions[] = { "1/4", "1/8", "1/16", "1/32" };

// slider1..slider5 of delay.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Time", "time", 0.0f, 2000.0f, 250.0f, "ms", 1.0f },
    { "Feedback", "feedback", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Tempo Sync", "sync", 0.0f, 1.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Subdivision", "subdivision", 0.0f, 3.0f, 0.0f, "", 1.0f, kNoteOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulldelay
