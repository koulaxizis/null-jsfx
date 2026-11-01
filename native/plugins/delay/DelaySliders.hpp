/*
 * NULL JSFX - Delay: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulldelay {

static const char* const kSyncOptions[] = { "OFF", "ON" };
static const char* const kNoteOptions[] = { "1/4", "1/8", "1/16", "1/32", "1/2", "1/1",
                                             "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" };
static const char* const kFreezeOptions[] = { "Off", "On" };
static const char* const kFreezeShort[] = { "OFF", "ON" };
static const char* const kModeOptions[] = { "Stereo", "Ping-Pong" };
static const char* const kModeShort[] = { "STEREO", "PING" };

// slider1..slider15 of delay.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Time", "time", 0.0f, 2000.0f, 250.0f, "ms", 1.0f },
    { "Feedback", "feedback", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mix", "mix", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Tempo Sync", "sync", 0.0f, 1.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Subdivision", "subdivision", 0.0f, 11.0f, 0.0f, "", 1.0f, kNoteOptions },
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Low Cut", "lowcut", 20.0f, 2000.0f, 20.0f, "Hz", 1.0f },
    { "High Cut", "highcut", 1000.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Drive", "drive", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Modulation", "modulation", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Diffusion", "diffusion", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Ducking", "ducking", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeOptions },
    { "Reverse", "reverse", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeOptions },
    { "Pitch", "pitch", -12.0f, 12.0f, 0.0f, "semitones", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the selector's upper-case mode names, for the interface
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0], kSliders[1], kSliders[2], kSliders[3], kSliders[4],
    { "Mode", "mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeShort },
    kSliders[6], kSliders[7], kSliders[8], kSliders[9], kSliders[10], kSliders[11],
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeShort },
    { "Reverse", "reverse", 0.0f, 1.0f, 0.0f, "", 1.0f, kFreezeShort },
    kSliders[14],
};

} // namespace nulldelay
