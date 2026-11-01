/*
 * NULL JSFX - Granular: the JSFX sliders (without the file) and the plugin base the interface
 * reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "GranularDSP.hpp"

#include <atomic>

namespace nullgrn {

// slider2..slider14 of granular.jsfx, in order (slider1, the file, is the "sample" state)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Position", "position", 0.0f, 100.0f, 30.0f, "%", 0.1f },
    { "Spray", "spray", 0.0f, 100.0f, 10.0f, "%", 0.1f },
    { "Scan", "scan", -100.0f, 100.0f, 0.0f, "%/s", 0.1f },
    { "Grain Size", "grain_size", 10.0f, 500.0f, 80.0f, "ms", 1.0f },
    { "Density", "density", 1.0f, 100.0f, 30.0f, "/s", 0.1f },
    { "Pitch Spread", "pitch_spread", 0.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Stereo Spread", "stereo_spread", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Reverse", "reverse", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Root Key", "root_key", 0.0f, 127.0f, 60.0f, "", 1.0f },
    { "Attack", "attack", 1.0f, 5000.0f, 200.0f, "ms", 1.0f },
    { "Release", "release", 1.0f, 10000.0f, 1000.0f, "ms", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullgrn

START_NAMESPACE_DISTRHO

// NullMultiPlugin with the "sample" state, plus what the display shows: the waveform of the
// sound that plays, whether it is a file, its channels, rate and length, and the grains
class GranularPluginBase : public NullMultiPlugin
{
public:
    GranularPluginBase(const SliderInfo* infos, uint32_t count)
        : NullMultiPlugin(infos, count, 1) {}

    float overview[nullgrn::kOverview] = {};
    std::atomic<bool> fileOk { false };
    std::atomic<int> channels { 1 };
    std::atomic<float> fileRate { 48000.0f }, seconds { 2.0f };
    // each grain: its place (part of the sound) and its window (0..1), -1 when silent
    float grainPos[nullgrn::kGrains] = {}, grainWin[nullgrn::kGrains] = {};
};

END_NAMESPACE_DISTRHO
