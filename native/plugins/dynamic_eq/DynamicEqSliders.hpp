/*
 * NULL JSFX - Dynamic EQ: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"

namespace nulldeq {

#define NULL_DEQ_BAND(n, lo, hi, def, step)                                    \
    { "B" n " Freq", "b" n "_freq", lo, hi, def, "Hz", step },                  \
    { "B" n " Gain", "b" n "_gain", -12.0f, 12.0f, 0.0f, "dB", 0.5f },          \
    { "B" n " Q", "b" n "_q", 0.5f, 4.0f, 1.0f, "", 0.1f },                     \
    { "B" n " Threshold", "b" n "_threshold", -36.0f, 0.0f, -12.0f, "dB", 1.0f }

// slider1..slider19 of dynamic_eq.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    NULL_DEQ_BAND("1", 20.0f, 1000.0f, 200.0f, 10.0f),
    NULL_DEQ_BAND("2", 200.0f, 4000.0f, 1000.0f, 100.0f),
    NULL_DEQ_BAND("3", 500.0f, 8000.0f, 2000.0f, 100.0f),
    NULL_DEQ_BAND("4", 2000.0f, 15000.0f, 6000.0f, 500.0f),
    { "Attack", "attack", 0.1f, 10.0f, 1.0f, "ms", 0.1f },
    { "Release", "release", 1.0f, 500.0f, 100.0f, "ms", 1.0f },
    { "Makeup", "makeup", 0.0f, 24.0f, 0.0f, "dB", 1.0f },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

#undef NULL_DEQ_BAND

// the parameters from the 19 slider values
template <typename Values>
inline Params params(const Values& value)
{
    Params p;
    for (int i = 0; i < kBands; ++i)
    {
        p.freq[i] = value(uint32_t(i * 4));
        p.gain[i] = value(uint32_t(i * 4 + 1));
        p.q[i] = value(uint32_t(i * 4 + 2));
        p.thresh[i] = value(uint32_t(i * 4 + 3));
    }
    p.attackMs = value(16);
    p.releaseMs = value(17);
    p.makeupDb = value(18);
    return p;
}

} // namespace nulldeq
