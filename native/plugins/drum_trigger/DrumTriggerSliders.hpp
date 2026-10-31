/*
 * NULL JSFX - Drum Trigger: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "DrumTriggerDSP.hpp"

namespace nulldrumtrigger {

static const char* const kOpt7[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };

// slider1..slider8 of drum_trigger.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Note", "note", 0.0f, 127.0f, 36.0f, "", 1.0f },
    { "Threshold", "threshold", -60.0f, 0.0f, -24.0f, "dB", 0.1f },
    { "Retrigger", "retrigger", 10.0f, 500.0f, 60.0f, "ms", 1.0f },
    { "Dynamics", "dynamics", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Detector HPF", "detector_hpf", 20.0f, 2000.0f, 20.0f, "Hz", 1.0f },
    { "Detector LPF", "detector_lpf", 50.0f, 20000.0f, 20000.0f, "Hz", 1.0f },
    { "Note Length", "note_length", 10.0f, 1000.0f, 50.0f, "ms", 1.0f },
    { "Channel", "channel", 0.0f, 15.0f, 9.0f, "", 1.0f, kOpt7 },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

static const DISTRHO::SliderInfo* const kUiSliders = kSliders;

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.note = int(v(0));
    p.threshold = v(1);
    p.retrigger = v(2);
    p.dynamics = v(3);
    p.hpf = v(4);
    p.lpf = v(5);
    p.length = v(6);
    p.channel = int(v(7));
    return p;
}

} // namespace nulldrumtrigger
