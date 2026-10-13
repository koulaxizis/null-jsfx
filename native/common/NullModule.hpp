/*
 * NULL JSFX - one plugin's DSP as a Rack module
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The Rack (plugins/rack) chains modules the way a REAPER FX chain chains JSFX. A module wraps
 * a plugin's framework-free DSP (plugins/<id>/<Name>DSP.hpp) exactly as that plugin's Plugin.cpp
 * drives it, so the module sounds like the plugin and the JSFX: valueChanged re-runs the JSFX
 * @slider code, process is @sample over a block. Every module has one value per JSFX slider,
 * in the JSFX's order, with the same ranges (its SliderInfo array). Modules are created on the
 * UI/main thread, never in process().
 */

#pragma once

#include "NullSliders.hpp"

#include <cstdint>

namespace nulljsfx {

// host transport for tempo-synced modules (the JSFX tempo / play_state / beat_position)
struct ModuleTime {
    double tempo = 120.0;
    double beat = 0.0;     // beat position at the start of the block
    bool playing = false;
    bool valid = false;    // the host reported a transport (DPF TimePosition::bbt.valid)
};

class Module {
public:
    virtual ~Module() {}

    // allocate for and reset to this sample rate (JSFX @init); called before the first process
    virtual void setSampleRate(double sampleRate) = 0;

    // slider `index` (0-based, JSFX order) changed to v; re-runs the JSFX @slider code
    virtual void setValue(uint32_t index, float v) = 0;

    // processes one block; in and out may be the same buffers
    virtual void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                         const ModuleTime& time) = 0;

    // latency in samples at the current settings (the JSFX pdc_delay), 0 for most modules
    virtual uint32_t latency() const { return 0; }
};

struct ModuleDesc {
    const char* id;        // the plugin's folder / JSFX file name, e.g. "delay"
    const char* name;      // e.g. "Delay"
    const char* category;  // "One Slider", "Essentials" or "Advanced"
    const DISTRHO::SliderInfo* sliders;
    uint32_t numSliders;
    Module* (*create)();
};

} // namespace nulljsfx
