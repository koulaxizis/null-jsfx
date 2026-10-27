/*
 * NULL JSFX - Convolution: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * NullMultiPlugin with one state, the IR file ("irfile"), plus the IR envelope / length the
 * display shows.
 */

#pragma once

#include "NullPlugin.hpp"
#include "ConvolutionDSP.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

class ConvolutionPluginBase : public NullMultiPlugin
{
public:
    ConvolutionPluginBase(const SliderInfo* infos, uint32_t count)
        : NullMultiPlugin(infos, count, 1) {}

    // what the display reads: the IR peak per 15 ms over 3 s, its length in seconds, and
    // whether it exists (an IR File that failed to load gives none)
    float envelope[nullconv::kEnvBins] = {};
    std::atomic<float> irSeconds { 0.0f };

protected:
    void initState(uint32_t index, State& state) override
    {
        if (index != 0)
            return;
        state.hints = kStateIsFilenamePath;
        state.key = "irfile";
        state.defaultValue = "";
        state.label = "IR File";
        state.description = "WAV impulse response used when Source is IR File";
    }
};

END_NAMESPACE_DISTRHO
