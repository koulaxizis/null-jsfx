/*
 * NULL JSFX - Downmix Check: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "DownmixCheckDSP.hpp"
#include "DownmixCheckSliders.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

// the plugin with what the interface shows: each bed pin's and each output's peak, and the
// fold's, since the interface last looked
class DownmixCheckBase : public NullMultiPlugin
{
public:
    DownmixCheckBase() : NullMultiPlugin(nulldmx::kSliders, nulldmx::kNumSliders) {}
    std::atomic<float> inPeak[12] {}, outPeak[12] {}, foldPeak { 0.0f };
};

// a peak the interface reads and clears: keep the larger
inline void keepPeak(std::atomic<float>& a, float v)
{
    if (v > a.load(std::memory_order_relaxed))
        a.store(v, std::memory_order_relaxed);
}

END_NAMESPACE_DISTRHO
