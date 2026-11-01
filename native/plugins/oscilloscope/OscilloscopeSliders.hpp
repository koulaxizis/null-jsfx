/*
 * NULL JSFX - Oscilloscope: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "OscilloscopeDSP.hpp"

namespace nullosc {

static const char* const kSyncOptions[] = { "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars" };
static const char* const kTriggerOptions[] = { "Auto", "Normal", "Free" };
static const char* const kEdgeOptions[] = { "Rising", "Falling" };
static const char* const kViewOptions[] = { "Mid", "Left/Right", "Side" };
static const char* const kOffOn[] = { "Off", "On" };
static const char* const kListenOptions[] = { "Stereo", "View" };

// slider1..slider9 of oscilloscope.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Time", "time", 1.0f, 2000.0f, 20.0f, "ms", 0.1f },
    { "Sync", "sync", 0.0f, 6.0f, 0.0f, "", 1.0f, kSyncOptions },
    { "Trigger", "trigger", 0.0f, 2.0f, 0.0f, "", 1.0f, kTriggerOptions },
    { "Edge", "edge", 0.0f, 1.0f, 0.0f, "", 1.0f, kEdgeOptions },
    { "Trigger Level", "trigger_level", -1.0f, 1.0f, 0.0f, "", 0.01f },
    { "View", "view", 0.0f, 2.0f, 0.0f, "", 1.0f, kViewOptions },
    { "Gain", "gain", -24.0f, 24.0f, 0.0f, "dB", 0.5f },
    { "Hold", "hold", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the same sliders with the interface's labels for the selectors
static const char* const kUiSync[] = { "OFF", "1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BARS" };
static const char* const kUiTrigger[] = { "AUTO", "NORMAL", "FREE" };
static const char* const kUiEdge[] = { "RISING", "FALLING" };
static const char* const kUiView[] = { "MID", "L / R", "SIDE" };
static const char* const kUiOffOn[] = { "OFF", "ON" };
static const char* const kUiListen[] = { "STEREO", "VIEW" };
static const DISTRHO::SliderInfo kUiSliders[] = {
    kSliders[0],
    { "Sync", "sync", 0.0f, 6.0f, 0.0f, "", 1.0f, kUiSync },
    { "Trigger", "trigger", 0.0f, 2.0f, 0.0f, "", 1.0f, kUiTrigger },
    { "Edge", "edge", 0.0f, 1.0f, 0.0f, "", 1.0f, kUiEdge },
    kSliders[4],
    { "View", "view", 0.0f, 2.0f, 0.0f, "", 1.0f, kUiView },
    kSliders[6],
    { "Hold", "hold", 0.0f, 1.0f, 0.0f, "", 1.0f, kUiOffOn },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kUiListen },
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters (the JSFX's update(): floor(x + 0.5) and >= 0.5)
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    p.time = v(0);
    p.sync = int(std::floor(v(1) + 0.5));
    p.trigger = int(std::floor(v(2) + 0.5));
    p.falling = v(3) >= 0.5;
    p.level = v(4);
    p.view = int(std::floor(v(5) + 0.5));
    p.gain = v(6);
    p.hold = v(7) >= 0.5;
    p.listen = v(8) >= 0.5;
    return p;
}

} // namespace nullosc

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the screen
class OscilloscopeBase : public NullMultiPlugin
{
public:
    OscilloscopeBase() : NullMultiPlugin(nullosc::kSliders, nullosc::kNumSliders) {}
    nullosc::View view;
};

END_NAMESPACE_DISTRHO
