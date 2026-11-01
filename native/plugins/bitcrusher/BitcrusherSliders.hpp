/*
 * NULL JSFX - Bitcrusher: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "BitcrusherDSP.hpp"

namespace nullbitcrusher {

static const char* const kOpt2[] = { "Off", "On" };

// slider1..slider7 of bitcrusher.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Bits", "bits", 1.0f, 16.0f, 8.0f, "", 1.0f },
    { "Sample Rate", "sample_rate", 100.0f, 48000.0f, 8000.0f, "Hz", 1.0f },
    { "Anti Alias", "anti_alias", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt2 },
    { "Smooth", "smooth", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Drive", "drive", 0.0f, 24.0f, 0.0f, "dB", 0.1f },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi2[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    kSliders[1],
    { "Anti Alias", "anti_alias", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi2 },
    kSliders[3],
    kSliders[4],
    kSliders[5],
    kSliders[6],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.bits = v(0);
    p.rate = v(1);
    p.antiAlias = int(v(2));
    p.smooth = v(3);
    p.drive = v(4);
    p.mix = v(5);
    p.output = v(6);
    return p;
}

} // namespace nullbitcrusher
