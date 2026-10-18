/*
 * NULL JSFX - Parametric EQ: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "EqAnalyzer.hpp"
#include "ParametricEqSliders.hpp"

START_NAMESPACE_DISTRHO

// the plugin with the rings of its input and output the interface draws as a spectrum
class ParametricEqBase : public NullMultiPlugin
{
public:
    ParametricEqBase() : NullMultiPlugin(nullpeq::kSliders, nullpeq::kNumSliders) {}
    nulleqan::Tap analyzer;
};

END_NAMESPACE_DISTRHO
