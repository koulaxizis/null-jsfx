/*
 * NULL JSFX - Mallets: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullmallets {

static const char* const kModelOptions[] = { "Marimba", "Xylophone", "Vibraphone", "Glockenspiel", "Tubular Bells", "Timpani" };
static const char* const kModelShort[] = { "MARIMBA", "XYLOPHONE", "VIBES", "GLOCKEN", "BELLS", "TIMPANI" };
static const char* const kKeyUpOptions[] = { "Ring", "Damp" };
static const char* const kKeyUpShort[] = { "RING", "DAMP" };

// slider1..slider11 of mallets.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Model", "model", 0.0f, 5.0f, 0.0f, "", 1.0f, kModelOptions },
    { "Mallet", "mallet", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Strike", "strike", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Resonator", "resonator", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Decay", "decay", 10.0f, 400.0f, 100.0f, "%", 1.0f },
    { "Key Up", "key_up", 0.0f, 1.0f, 0.0f, "", 1.0f, kKeyUpOptions },
    { "Damp Time", "damp_time", 10.0f, 3000.0f, 150.0f, "ms", 1.0f },
    { "Motor", "motor", 0.0f, 12.0f, 0.0f, "Hz", 0.1f },
    { "Velocity", "velocity", 0.0f, 100.0f, 60.0f, "%", 1.0f },
    { "Width", "width", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullmallets
