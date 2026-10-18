/*
 * NULL JSFX - Percussion: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullperc {

static const char* const kGrooveOptions[] = { "Off", "Salsa", "Bossa", "Samba", "Cumbia", "Afro Bell", "Afrobeat", "Reggaeton", "Pop" };
static const char* const kGrooveShort[] = { "OFF", "SALSA", "BOSSA", "SAMBA", "CUMBIA", "AFRO BELL", "AFROBEAT", "REGGAETON", "POP" };
static const char* const kGroovePlayOptions[] = { "Transport", "Keys" };
static const char* const kGroovePlayShort[] = { "TRANSPORT", "KEYS" };

// slider1..slider52 of percussion.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Groove", "groove", 0.0f, 8.0f, 1.0f, "", 1.0f, kGrooveOptions },
    { "Groove Play", "groove_play", 0.0f, 1.0f, 0.0f, "", 1.0f, kGroovePlayOptions },
    { "Swing", "swing", 50.0f, 75.0f, 50.0f, "%", 1.0f },
    { "Accent", "accent", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Humanize", "humanize", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Room", "room", 0.0f, 100.0f, 15.0f, "%", 1.0f },
    { "Room Size", "room_size", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Conga Level", "conga_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Conga Tune", "conga_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Conga Decay", "conga_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Conga Pan", "conga_pan", -100.0f, 100.0f, 15.0f, "%", 1.0f },
    { "Tumba Level", "tumba_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Tumba Tune", "tumba_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Tumba Decay", "tumba_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Tumba Pan", "tumba_pan", -100.0f, 100.0f, -15.0f, "%", 1.0f },
    { "Bongo Hi Level", "bongo_hi_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Bongo Hi Tune", "bongo_hi_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Bongo Hi Decay", "bongo_hi_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Bongo Hi Pan", "bongo_hi_pan", -100.0f, 100.0f, 35.0f, "%", 1.0f },
    { "Bongo Lo Level", "bongo_lo_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Bongo Lo Tune", "bongo_lo_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Bongo Lo Decay", "bongo_lo_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Bongo Lo Pan", "bongo_lo_pan", -100.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Shaker Level", "shaker_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Shaker Tune", "shaker_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Shaker Decay", "shaker_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Shaker Pan", "shaker_pan", -100.0f, 100.0f, -35.0f, "%", 1.0f },
    { "Tambourine Level", "tambourine_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Tambourine Tune", "tambourine_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Tambourine Decay", "tambourine_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Tambourine Pan", "tambourine_pan", -100.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Cowbell Level", "cowbell_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Cowbell Tune", "cowbell_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Cowbell Decay", "cowbell_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Cowbell Pan", "cowbell_pan", -100.0f, 100.0f, -20.0f, "%", 1.0f },
    { "Claves Level", "claves_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Claves Tune", "claves_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Claves Decay", "claves_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Claves Pan", "claves_pan", -100.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Guiro Level", "guiro_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Guiro Tune", "guiro_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Guiro Decay", "guiro_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Guiro Pan", "guiro_pan", -100.0f, 100.0f, -45.0f, "%", 1.0f },
    { "Cabasa Level", "cabasa_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Cabasa Tune", "cabasa_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Cabasa Decay", "cabasa_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Cabasa Pan", "cabasa_pan", -100.0f, 100.0f, 45.0f, "%", 1.0f },
    { "Triangle Level", "triangle_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Triangle Tune", "triangle_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Triangle Decay", "triangle_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Triangle Pan", "triangle_pan", -100.0f, 100.0f, -30.0f, "%", 1.0f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullperc
