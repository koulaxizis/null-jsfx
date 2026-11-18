/*
 * NULL JSFX - Beat Gen: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"
#include "BeatGenDSP.hpp"

namespace nullbeatgen {

static const char* const kOffOn[] = { "Off", "On" };
static const char* const kStyle[] = { "Four on Floor", "Breakbeat", "Hip Hop", "Trap", "Random" };
static const char* const kRate[] = { "1/8", "1/16", "1/32", "1/16T" };
static const char* const kEvery[] = { "1 Cycle", "2 Cycles", "4 Cycles", "8 Cycles" };
static const char* const kRun[] = { "Transport", "Always" };
static const char* const kChannel[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };

// slider1..slider43 of beat_gen.jsfx, in order: per lane Density (1-8), Steps (9-16), Note (17-24) and
// Lock (25-32), then the pattern's settings
static const DISTRHO::SliderInfo kSliders[] = {
    { "Kick Density", "kick_density", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Snare Density", "snare_density", 0.0f, 100.0f, 40.0f, "%", 1.0f },
    { "Closed Hat Density", "closed_hat_density", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Open Hat Density", "open_hat_density", 0.0f, 100.0f, 15.0f, "%", 1.0f },
    { "Clap Density", "clap_density", 0.0f, 100.0f, 15.0f, "%", 1.0f },
    { "Tom Density", "tom_density", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Perc Density", "perc_density", 0.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Cymbal Density", "cymbal_density", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Kick Steps", "kick_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Snare Steps", "snare_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Closed Hat Steps", "closed_hat_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Open Hat Steps", "open_hat_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Clap Steps", "clap_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Tom Steps", "tom_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Perc Steps", "perc_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Cymbal Steps", "cymbal_steps", 1.0f, 16.0f, 16.0f, "", 1.0f },
    { "Kick Note", "kick_note", 0.0f, 127.0f, 36.0f, "", 1.0f },
    { "Snare Note", "snare_note", 0.0f, 127.0f, 38.0f, "", 1.0f },
    { "Closed Hat Note", "closed_hat_note", 0.0f, 127.0f, 42.0f, "", 1.0f },
    { "Open Hat Note", "open_hat_note", 0.0f, 127.0f, 46.0f, "", 1.0f },
    { "Clap Note", "clap_note", 0.0f, 127.0f, 39.0f, "", 1.0f },
    { "Tom Note", "tom_note", 0.0f, 127.0f, 45.0f, "", 1.0f },
    { "Perc Note", "perc_note", 0.0f, 127.0f, 37.0f, "", 1.0f },
    { "Cymbal Note", "cymbal_note", 0.0f, 127.0f, 51.0f, "", 1.0f },
    { "Kick Lock", "kick_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Snare Lock", "snare_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Closed Hat Lock", "closed_hat_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Open Hat Lock", "open_hat_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Clap Lock", "clap_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Tom Lock", "tom_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Perc Lock", "perc_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Cymbal Lock", "cymbal_lock", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Style", "style", 0.0f, 4.0f, 0.0f, "", 1.0f, kStyle },
    { "Seed", "seed", 1.0f, 999.0f, 1.0f, "", 1.0f },
    { "Rate", "rate", 0.0f, 3.0f, 1.0f, "", 1.0f, kRate },
    { "Mutate", "mutate", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Mutate Every", "mutate_every", 0.0f, 3.0f, 1.0f, "", 1.0f, kEvery },
    { "Velocity", "velocity", 1.0f, 127.0f, 100.0f, "", 1.0f },
    { "Velocity Range", "velocity_range", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Swing", "swing", 0.0f, 75.0f, 0.0f, "%", 1.0f },
    { "Gate", "gate", 5.0f, 95.0f, 50.0f, "%", 1.0f },
    { "Run", "run", 0.0f, 1.0f, 0.0f, "", 1.0f, kRun },
    { "Channel", "channel", 0.0f, 15.0f, 9.0f, "", 1.0f, kChannel },
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

// the interface's labels for the selectors
static const char* const kUiOffOn[] = { "OFF", "ON" };
static const char* const kUiStyle[] = { "4 ON FLOOR", "BREAKBEAT", "HIP HOP", "TRAP", "RANDOM" };
static const char* const kUiEvery[] = { "1 CYCLE", "2 CYCLES", "4 CYCLES", "8 CYCLES" };
static const char* const kUiRun[] = { "HOST", "ALWAYS" };

struct UiSliders {
    DISTRHO::SliderInfo s[kNumSliders];
    UiSliders()
    {
        for (uint32_t i = 0; i < kNumSliders; ++i)
            s[i] = kSliders[i];
        for (uint32_t i = 24; i < 32; ++i)
            s[i].options = kUiOffOn;
        s[32].options = kUiStyle;
        s[36].options = kUiEvery;
        s[41].options = kUiRun;
    }
};

// the JSFX's double for a host float (NullSliders.hpp)
inline double sliderValue(uint32_t i, float v) noexcept { return DISTRHO::jsfxValue(kSliders[i], v); }

// every value as the DSP's parameters
template <class V>
inline Params paramsFrom(V&& v)
{
    Params p;
    for (int l = 0; l < kLanes; ++l)
    {
        p.density[l] = v(uint32_t(l));
        p.steps[l] = int(v(uint32_t(8 + l)));
        p.note[l] = int(v(uint32_t(16 + l)));
        p.lock[l] = v(uint32_t(24 + l)) >= 0.5;
    }
    p.style = int(v(32));
    p.seed = v(33);
    p.rate = int(v(34));
    p.mutPct = v(35);
    p.mutEvery = int(v(36));
    p.velocity = v(37);
    p.velRange = v(38);
    p.swing = v(39);
    p.gate = v(40);
    p.run = int(v(41));
    p.channel = int(v(42));
    return p;
}

} // namespace nullbeatgen
