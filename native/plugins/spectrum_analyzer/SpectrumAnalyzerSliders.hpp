/*
 * NULL JSFX - Spectrum Analyzer: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "SpectrumAnalyzerDSP.hpp"

namespace nullspec {

static const char* const kSizeOptions[] = { "1024", "2048", "4096", "8192", "16384" };
static const char* const kRangeOptions[] = { "60 DB", "80 DB", "100 DB", "120 DB" };
static const char* const kHoldOptions[] = { "OFF", "2 S", "INFINITE" };
static const char* const kChannelOptions[] = { "MID", "LEFT", "RIGHT", "SIDE" };
static const char* const kMonitorOptions[] = { "STEREO", "SOLO" };

// slider1..slider7 of spectrum_analyzer.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "FFT Size", "fft_size", 0.0f, 4.0f, 2.0f, "", 1.0f, kSizeOptions },
    { "Slope", "slope", 0.0f, 6.0f, 4.5f, "dB/oct", 0.5f },
    { "Release", "release", 0.0f, 2000.0f, 300.0f, "ms", 10.0f },
    { "Range", "range", 0.0f, 3.0f, 2.0f, "", 1.0f, kRangeOptions },
    { "Peak Hold", "peak_hold", 0.0f, 2.0f, 1.0f, "", 1.0f, kHoldOptions },
    { "Channel", "channel", 0.0f, 3.0f, 0.0f, "", 1.0f, kChannelOptions },
    { "Monitor", "monitor", 0.0f, 1.0f, 0.0f, "", 1.0f, kMonitorOptions },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullspec

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the spectrum
class SpectrumAnalyzerBase : public NullMultiPlugin
{
public:
    SpectrumAnalyzerBase() : NullMultiPlugin(nullspec::kSliders, nullspec::kNumSliders) {}
    nullspec::View view;
};

END_NAMESPACE_DISTRHO
