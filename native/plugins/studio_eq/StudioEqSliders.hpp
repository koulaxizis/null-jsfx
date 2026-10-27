/*
 * NULL JSFX - Studio EQ: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullseq {

static const char* const kTypeOptions[] = { "OFF", "BELL", "LO SHELF", "HI SHELF", "LOW CUT", "HIGH CUT", "NOTCH", "BAND" };
static const char* const kSlopeOptions[] = { "6 dB", "12 dB", "18 dB", "24 dB", "36 dB", "48 dB", "72 dB", "96 dB" };
static const char* const kChannelOptions[] = { "STEREO", "MID", "SIDE", "LEFT", "RIGHT" };
static const char* const kOffOnOptions[] = { "OFF", "ON" };
static const char* const kPhaseOptions[] = { "ZERO LAT", "LINEAR" };

#define NULL_SEQ_BAND(n, type, freq)                                                       \
    { "Band " n " Type", "band" n "_type", 0.0f, 7.0f, type, "", 1.0f, kTypeOptions },        \
    { "Band " n " Freq", "band" n "_freq", 10.0f, 30000.0f, freq, "Hz", 1.0f },               \
    { "Band " n " Gain", "band" n "_gain", -30.0f, 30.0f, 0.0f, "dB", 0.1f },                 \
    { "Band " n " Q", "band" n "_q", 0.1f, 18.0f, 0.71f, "", 0.01f },                         \
    { "Band " n " Slope", "band" n "_slope", 0.0f, 7.0f, 3.0f, "", 1.0f, kSlopeOptions },      \
    { "Band " n " Channels", "band" n "_channels", 0.0f, 4.0f, 0.0f, "", 1.0f, kChannelOptions }

// slider1..slider51 of studio_eq.jsfx, in order: six per band, then Output, Auto Gain, Phase
// (Freq and Q are log-shaped in the JSFX)
static const DISTRHO::SliderInfo kSliders[] = {
    NULL_SEQ_BAND("1", 0.0f, 30.0f),
    NULL_SEQ_BAND("2", 1.0f, 100.0f),
    NULL_SEQ_BAND("3", 1.0f, 250.0f),
    NULL_SEQ_BAND("4", 1.0f, 600.0f),
    NULL_SEQ_BAND("5", 1.0f, 1500.0f),
    NULL_SEQ_BAND("6", 1.0f, 3500.0f),
    NULL_SEQ_BAND("7", 1.0f, 8000.0f),
    NULL_SEQ_BAND("8", 0.0f, 18000.0f),
    { "Output", "output", -24.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Auto Gain", "auto_gain", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOnOptions },
    { "Phase", "phase", 0.0f, 1.0f, 0.0f, "", 1.0f, kPhaseOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);
static_assert(kNumSliders == 51, "51 sliders");

#undef NULL_SEQ_BAND

static constexpr uint32_t kOutput = 48, kAutoGain = 49, kPhase = 50;

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

} // namespace nullseq
