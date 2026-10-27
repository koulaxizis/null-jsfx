/*
 * NULL JSFX - Reference: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "../parametric_eq/EqAnalyzer.hpp"
#include "ReferenceDSP.hpp"
#include "ReferenceSliders.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

// the plugin with what the interface shows: the mono track (pre) and the reference at its
// played gain (post) for the spectra, and the numbers
class ReferenceBase : public NullMultiPlugin
{
public:
    ReferenceBase() : NullMultiPlugin(nullref::kSliders, nullref::kNumSliders) {}
    nulleqan::Tap analyzer;
    std::atomic<float> trackLufs { -200.0f }, referenceLufs { -200.0f }, referenceGain { 1.0f };
    std::atomic<float> trackCorr { 0.0f }, referenceCorr { 0.0f };
};

END_NAMESPACE_DISTRHO
