/*
 * NULL JSFX - Spectral Ducker: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "SpectralDuckerDSP.hpp"
#include "SpectralDuckerSliders.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

// the plugin with what the interface draws: the gain reduction (dB) and the key level
// (amplitude) of each band, written by the audio thread after each block
class SpectralDuckerBase : public NullMultiPlugin
{
public:
    SpectralDuckerBase() : NullMultiPlugin(nullsduck::kSliders, nullsduck::kNumSliders) {}
    std::atomic<float> bandGr[nullsduck::kBands] = {}, bandKey[nullsduck::kBands] = {};
};

END_NAMESPACE_DISTRHO
