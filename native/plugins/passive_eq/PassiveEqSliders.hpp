/*
 * NULL JSFX - Passive EQ: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "PassiveEqDSP.hpp"

namespace nullpassiveeq {

static const char* const kLowFreqOptions[] = { "20 Hz", "30 Hz", "60 Hz", "100 Hz" };
static const char* const kHighFreqOptions[] = { "3 kHz", "4 kHz", "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" };
static const char* const kAttenFreqOptions[] = { "5 kHz", "10 kHz", "20 kHz" };

// slider1..slider10 of passive_eq.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Low Freq", "low_freq", 0.0f, 3.0f, 2.0f, "", 1.0f, kLowFreqOptions },
    { "Low Boost", "low_boost", 0.0f, 10.0f, 0.0f, "", 0.1f },
    { "Low Atten", "low_atten", 0.0f, 10.0f, 0.0f, "", 0.1f },
    { "High Freq", "high_freq", 0.0f, 6.0f, 3.0f, "", 1.0f, kHighFreqOptions },
    { "High Boost", "high_boost", 0.0f, 10.0f, 0.0f, "", 0.1f },
    { "Bandwidth", "bandwidth", 0.0f, 10.0f, 5.0f, "", 0.1f },
    { "Atten Freq", "atten_freq", 0.0f, 2.0f, 1.0f, "", 1.0f, kAttenFreqOptions },
    { "High Atten", "high_atten", 0.0f, 10.0f, 0.0f, "", 0.1f },
    { "Tube", "tube", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Output", "output", -12.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUiLowFreq[] = { "20", "30", "60", "100" };
static const char* const kUiHighFreq[] = { "3K", "4K", "5K", "8K", "10K", "12K", "16K" };
static const char* const kUiAttenFreq[] = { "5K", "10K", "20K" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Low Freq", "low_freq", 0.0f, 3.0f, 2.0f, "", 1.0f, kUiLowFreq },
    kSliders[1], kSliders[2],
    { "High Freq", "high_freq", 0.0f, 6.0f, 3.0f, "", 1.0f, kUiHighFreq },
    kSliders[4], kSliders[5],
    { "Atten Freq", "atten_freq", 0.0f, 2.0f, 1.0f, "", 1.0f, kUiAttenFreq },
    kSliders[7], kSliders[8], kSliders[9],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// all ten values as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.lowFreq = int(v(0));
    p.lowBoost = v(1);
    p.lowAtten = v(2);
    p.highFreq = int(v(3));
    p.highBoost = v(4);
    p.bandwidth = v(5);
    p.attenFreq = int(v(6));
    p.highAtten = v(7);
    p.tube = v(8);
    p.output = v(9);
    return p;
}

} // namespace nullpassiveeq
