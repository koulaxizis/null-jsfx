/*
 * NULL JSFX - Sampler: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * NullMultiPlugin with the "sample" state, plus what the display shows: the waveform of the
 * sound that plays, whether it is a file, its channels, rate and length.
 */

#pragma once

#include "NullPlugin.hpp"
#include "SamplerDSP.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

class SamplerPluginBase : public NullMultiPlugin
{
public:
    SamplerPluginBase(const SliderInfo* infos, uint32_t count)
        : NullMultiPlugin(infos, count, 1) {}

    float overview[nullsmp::kOverview] = {};
    std::atomic<bool> fileOk { false };
    std::atomic<int> channels { 1 };
    std::atomic<float> fileRate { 44100.0f }, seconds { 1.5f };
};

END_NAMESPACE_DISTRHO
