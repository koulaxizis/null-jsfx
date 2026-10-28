/*
 * NULL JSFX - Bass Management: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "BassManagementDSP.hpp"
#include "BassManagementSliders.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

// the plugin with what the interface shows: each output's peak since the interface last looked
class BassManagementBase : public NullMultiPlugin
{
public:
    BassManagementBase() : NullMultiPlugin(nullbm::kSliders, nullbm::kNumSliders) {}
    std::atomic<float> peak[12] {};
};

END_NAMESPACE_DISTRHO
