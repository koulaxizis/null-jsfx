/*
 * NULL JSFX - Organ: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullorgan {

static const char* const kOnOptions[] = { "Off", "On" };
static const char* const kOnShort[] = { "OFF", "ON" };
static const char* const kHarmOptions[] = { "Second", "Third" };
static const char* const kHarmShort[] = { "SECOND", "THIRD" };
static const char* const kDecayOptions[] = { "Fast", "Slow" };
static const char* const kDecayShort[] = { "FAST", "SLOW" };
static const char* const kPercVolOptions[] = { "Normal", "Soft" };
static const char* const kPercVolShort[] = { "NORMAL", "SOFT" };
static const char* const kVibOptions[] = { "Off", "V1", "V2", "V3", "C1", "C2", "C3" };
static const char* const kVibShort[] = { "OFF", "V1", "V2", "V3", "C1", "C2", "C3" };
static const char* const kRotOptions[] = { "Off", "Slow", "Fast", "Stop" };
static const char* const kRotShort[] = { "OFF", "SLOW", "FAST", "STOP" };

// slider1..slider21 of organ.jsfx, in order
static const char* const kVoiceOptions[] = { "Tonewheel", "Pipe" };
static const char* const kVoiceShort[] = { "TONEWHEEL", "PIPE" };

static const DISTRHO::SliderInfo kSliders[] = {
    { "Drawbar 16'", "drawbar_16", 0.0f, 8.0f, 8.0f, "", 1.0f },
    { "Drawbar 5 1/3'", "drawbar_5_1_3", 0.0f, 8.0f, 8.0f, "", 1.0f },
    { "Drawbar 8'", "drawbar_8", 0.0f, 8.0f, 8.0f, "", 1.0f },
    { "Drawbar 4'", "drawbar_4", 0.0f, 8.0f, 0.0f, "", 1.0f },
    { "Drawbar 2 2/3'", "drawbar_2_2_3", 0.0f, 8.0f, 0.0f, "", 1.0f },
    { "Drawbar 2'", "drawbar_2", 0.0f, 8.0f, 0.0f, "", 1.0f },
    { "Drawbar 1 3/5'", "drawbar_1_3_5", 0.0f, 8.0f, 0.0f, "", 1.0f },
    { "Drawbar 1 1/3'", "drawbar_1_1_3", 0.0f, 8.0f, 0.0f, "", 1.0f },
    { "Drawbar 1'", "drawbar_1", 0.0f, 8.0f, 0.0f, "", 1.0f },
    { "Percussion", "percussion", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOptions },
    { "Perc Harmonic", "perc_harmonic", 0.0f, 1.0f, 1.0f, "", 1.0f, kHarmOptions },
    { "Perc Decay", "perc_decay", 0.0f, 1.0f, 0.0f, "", 1.0f, kDecayOptions },
    { "Perc Volume", "perc_volume", 0.0f, 1.0f, 0.0f, "", 1.0f, kPercVolOptions },
    { "Key Click", "key_click", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Vibrato", "vibrato", 0.0f, 6.0f, 6.0f, "", 1.0f, kVibOptions },
    { "Drive", "drive", 0.0f, 100.0f, 15.0f, "%", 1.0f },
    { "Rotary", "rotary", 0.0f, 3.0f, 1.0f, "", 1.0f, kRotOptions },
    { "Acceleration", "acceleration", 10.0f, 400.0f, 100.0f, "%", 1.0f },
    { "Horn/Drum Balance", "horn_drum_balance", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Voice", "voice", 0.0f, 1.0f, 0.0f, "", 1.0f, kVoiceOptions },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullorgan
