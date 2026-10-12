/*
 * NULL JSFX - Parametric EQ: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nullpeq {

static const char* const kTypeOptions[] = { "L SHELF", "H SHELF", "PEAK", "LO PASS", "HI PASS", "NOTCH", "BAND", "ALLPASS" };
static const char* const kModeOptions[] = { "ACTIVE", "BYPASS", "SOLO" };

#define NULL_PEQ_BAND(name, sym, lo, hi, def)                                              \
    { name " Freq", sym "_freq", lo, hi, def, "Hz", 1.0f },                                 \
    { name " Gain", sym "_gain", -18.0f, 18.0f, 0.0f, "dB", 0.1f },                         \
    { name " Q", sym "_q", 0.1f, 10.0f, 1.0f, "", 0.1f },                                   \
    { name " Type", sym "_type", 0.0f, 7.0f, 2.0f, "", 1.0f, kTypeOptions },                \
    { name " Mode", sym "_mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kModeOptions }

// slider1..slider25 of parametric_eq.jsfx, in order: five sliders per band
static const DISTRHO::SliderInfo kSliders[] = {
    NULL_PEQ_BAND("Low", "low", 20.0f, 300.0f, 77.0f),
    NULL_PEQ_BAND("Low Mid", "low_mid", 300.0f, 1000.0f, 548.0f),
    NULL_PEQ_BAND("Mid High", "mid_high", 1000.0f, 5000.0f, 2236.0f),
    NULL_PEQ_BAND("High", "high", 5000.0f, 20000.0f, 10000.0f),
    NULL_PEQ_BAND("Ultra High", "ultra_high", 10000.0f, 24000.0f, 15000.0f),
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

#undef NULL_PEQ_BAND

// the band parameters from the 25 slider values
template <typename Values>
inline void bandParams(const Values& value, BandParams* out)
{
    for (int i = 0; i < kBands; ++i)
    {
        out[i].freq = value(uint32_t(i * 5));
        out[i].gain = value(uint32_t(i * 5 + 1));
        out[i].q = value(uint32_t(i * 5 + 2));
        out[i].type = int(std::lround(value(uint32_t(i * 5 + 3))));
        out[i].mode = int(std::lround(value(uint32_t(i * 5 + 4))));
    }
}

} // namespace nullpeq
