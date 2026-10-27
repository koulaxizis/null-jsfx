/*
 * NULL JSFX - Studio EQ: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "../parametric_eq/EqAnalyzer.hpp"
#include "StudioEqSliders.hpp"

START_NAMESPACE_DISTRHO

// the plugin with the rings of its input and output the interface draws as a spectrum
class StudioEqBase : public NullMultiPlugin
{
public:
    StudioEqBase() : NullMultiPlugin(nullseq::kSliders, nullseq::kNumSliders) {}
    nulleqan::Tap analyzer;
};

END_NAMESPACE_DISTRHO
