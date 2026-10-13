/*
 * NULL JSFX - Trance Gate: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "TranceGateDSP.hpp"

#include <cmath>

namespace nulltrance {

static const char* const kOnOff[] = { "OFF", "ON" };
static const char* const kModeOptions[] = { "SYNC", "FREE" };
static const char* const kNoteOptions[] = { "1/4", "1/8", "1/16", "1/32" };

// slider1..slider22 of trance_gate.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Step 1", "step1", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 2", "step2", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 3", "step3", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Step 4", "step4", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 5", "step5", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 6", "step6", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Step 7", "step7", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Step 8", "step8", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 9", "step9", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 10", "step10", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Step 11", "step11", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Step 12", "step12", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 13", "step13", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Step 14", "step14", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Step 15", "step15", 0.0f, 1.0f, 0.0f, "", 1.0f, kOnOff },
    { "Step 16", "step16", 0.0f, 1.0f, 1.0f, "", 1.0f, kOnOff },
    { "Rate Mode", "rate_mode", 0.0f, 1.0f, 0.0f, "", 1.0f, kModeOptions },
    { "Step Length", "step_length", 0.0f, 3.0f, 2.0f, "", 1.0f, kNoteOptions },
    { "Free Rate", "free_rate", 0.5f, 40.0f, 8.0f, "Hz", 0.1f },
    { "Attack", "attack", 1.0f, 100.0f, 5.0f, "ms", 1.0f },
    { "Release", "release", 10.0f, 500.0f, 40.0f, "ms", 1.0f },
    { "Gate Depth", "depth", 0.0f, 100.0f, 100.0f, "%", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// a 0.1-step slider value as the JSFX sees it (the double nearest the decimal), not the float
inline double tenths(float v) { return std::round(double(v) * 10.0) / 10.0; }

// the parameter values as DSP settings
template <typename V>
inline Params paramsFrom(V value)
{
    Params p;
    for (int k = 0; k < 16; ++k)
        p.steps[k] = value(uint32_t(k)) >= 0.5f;
    p.freeMode = value(16) >= 0.5f;
    p.stepLength = int(std::lround(value(17)));
    p.freeRate = tenths(value(18));
    p.attackMs = std::round(value(19));
    p.releaseMs = std::round(value(20));
    p.depthPct = std::round(value(21));
    return p;
}

} // namespace nulltrance
