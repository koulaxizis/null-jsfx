/*
 * NULL JSFX - Speaker Sim: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "SpeakerSimDSP.hpp"

namespace nullspeakersim {

static const char* const kOpt0[] = { "Phone", "Laptop", "Earbuds", "TV", "Radio", "Telephone", "Megaphone", "Car" };

// slider1..slider4 of speaker_sim.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Device", "device", 0.0f, 7.0f, 0.0f, "", 1.0f, kOpt0 },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Drive", "drive", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Output", "output", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi0[] = { "PHONE", "LAPTOP", "EARBUDS", "TV", "RADIO", "TELEPHONE", "MEGAPHONE", "CAR" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Device", "device", 0.0f, 7.0f, 0.0f, "", 1.0f, kUi0 },
    kSliders[1],
    kSliders[2],
    kSliders[3],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.device = int(v(0));
    p.mix = v(1);
    p.drive = v(2);
    p.output = v(3);
    return p;
}

} // namespace nullspeakersim
