/*
 * NULL JSFX - MIDI Monitor: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "MidiMonitorDSP.hpp"

namespace nullmidimonitor {

static const char* const kOpt0[] = { "Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
static const char* const kOpt1[] = { "All", "Notes", "Controllers", "Other" };
static const char* const kOpt2[] = { "Off", "On" };
static const char* const kOpt3[] = { "C4", "C3" };

// slider1..slider4 of midi_monitor.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Channel", "channel", 0.0f, 16.0f, 0.0f, "", 1.0f, kOpt0 },
    { "Show", "show", 0.0f, 3.0f, 0.0f, "", 1.0f, kOpt1 },
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt2 },
    { "Middle C", "middle_c", 0.0f, 1.0f, 0.0f, "", 1.0f, kOpt3 },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUi0[] = { "OMNI", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
static const char* const kUi1[] = { "ALL", "NOTES", "CONTROLLERS", "OTHER" };
static const char* const kUi2[] = { "OFF", "ON" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    { "Channel", "channel", 0.0f, 16.0f, 0.0f, "", 1.0f, kUi0 },
    { "Show", "show", 0.0f, 3.0f, 0.0f, "", 1.0f, kUi1 },
    { "Freeze", "freeze", 0.0f, 1.0f, 0.0f, "", 1.0f, kUi2 },
    kSliders[3],
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.channel = int(v(0));
    p.show = int(v(1));
    p.freeze = int(v(2));
    p.middleC = int(v(3));
    return p;
}

// whether message status a passes Channel and Show (the list and the keyboard)
inline bool shown(const Params& p, int a) noexcept
{
    if (p.channel > 0 && (a & 15) != p.channel - 1)
        return false;
    const int st = a & 0xF0;
    if (p.show == 1)
        return st == 0x80 || st == 0x90 || st == 0xA0;
    if (p.show == 2)
        return st == 0xB0 || st == 0xD0 || st == 0xE0;
    if (p.show == 3)
        return st == 0xC0;
    return true;
}

} // namespace nullmidimonitor
