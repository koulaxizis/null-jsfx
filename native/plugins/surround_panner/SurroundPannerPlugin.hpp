/*
 * NULL JSFX - Surround Panner: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "SurroundPannerDSP.hpp"
#include "SurroundPannerSliders.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

// the plugin with what the interface shows: each pin's gain (relative to Output) and its peak
// since the interface last looked
class SurroundPannerBase : public NullMultiPlugin
{
public:
    SurroundPannerBase() : NullMultiPlugin(nullpan::kSliders, nullpan::kNumSliders) {}
    std::atomic<float> power[nullpan::kPins] {}, peak[nullpan::kPins] {};
};

END_NAMESPACE_DISTRHO
