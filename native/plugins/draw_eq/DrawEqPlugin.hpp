/*
 * NULL JSFX - Draw EQ: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "DrawEqSliders.hpp"
#include "../parametric_eq/EqAnalyzer.hpp"

START_NAMESPACE_DISTRHO

// the plugin with the ring of its output the interface draws as a spectrum
class DrawEqBase : public NullMultiPlugin
{
public:
    DrawEqBase() : NullMultiPlugin(nulldraweq::kSliders, nulldraweq::kNumSliders) {}
    nulleqan::Tap analyzer;
};

END_NAMESPACE_DISTRHO
