/*
 * NULL JSFX - Synth: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nullsynth {

static const char* const kEngineOptions[] = { "Analog", "Wavetable" };
static const char* const kEngineShort[] = { "ANALOG", "WAVE" };
static const char* const kOsc1WaveOptions[] = { "Saw", "Square", "Triangle", "Sine" };
static const char* const kOsc1WaveShort[] = { "SAW", "SQUARE", "TRIANGLE", "SINE" };
static const char* const kOsc2WaveOptions[] = { "Saw", "Square", "Triangle", "Sine" };
static const char* const kOsc2WaveShort[] = { "SAW", "SQUARE", "TRIANGLE", "SINE" };
static const char* const kTableOptions[] = { "Basic", "Pulse", "Organ", "Vocal", "Glass", "Sync", "Fold", "Digital" };
static const char* const kTableShort[] = { "BASIC", "PULSE", "ORGAN", "VOCAL", "GLASS", "SYNC", "FOLD", "DIGITAL" };
static const char* const kFilterModeOptions[] = { "Low Pass", "Band Pass", "High Pass" };
static const char* const kFilterModeShort[] = { "LOW PASS", "BAND PASS", "HIGH PASS" };
static const char* const kSlopeOptions[] = { "12 dB", "24 dB" };
static const char* const kSlopeShort[] = { "12 DB", "24 DB" };
static const char* const kLfoShapeOptions[] = { "Sine", "Triangle", "Square", "Saw", "Random" };
static const char* const kLfoShapeShort[] = { "SINE", "TRIANGLE", "SQUARE", "SAW", "RANDOM" };
static const char* const kLfoSyncOptions[] = { "Off", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4D", "1/8D" };
static const char* const kLfoSyncShort[] = { "OFF", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4D", "1/8D" };
static const char* const kVoiceModeOptions[] = { "Poly", "Mono", "Legato" };
static const char* const kVoiceModeShort[] = { "POLY", "MONO", "LEGATO" };
static const char* const kDelayTimeOptions[] = { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" };
static const char* const kDelayTimeShort[] = { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" };
static const char* const kMpeOptions[] = { "Off", "On" };
static const char* const kMpeShort[] = { "OFF", "ON" };

// slider1..slider45 of synth.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Engine", "engine", 0.0f, 1.0f, 0.0f, "", 1.0f, kEngineOptions },
    { "Osc 1 Wave", "osc_1_wave", 0.0f, 3.0f, 0.0f, "", 1.0f, kOsc1WaveOptions },
    { "Osc 2 Wave", "osc_2_wave", 0.0f, 3.0f, 0.0f, "", 1.0f, kOsc2WaveOptions },
    { "Osc 2 Semi", "osc_2_semi", -24.0f, 24.0f, 0.0f, "st", 1.0f },
    { "Osc 2 Detune", "osc_2_detune", -50.0f, 50.0f, 7.0f, "ct", 1.0f },
    { "Osc Mix", "osc_mix", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Sub", "sub", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Noise", "noise", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Table", "table", 0.0f, 7.0f, 0.0f, "", 1.0f, kTableOptions },
    { "Position", "position", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Filter Mode", "filter_mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kFilterModeOptions },
    { "Slope", "slope", 0.0f, 1.0f, 1.0f, "", 1.0f, kSlopeOptions },
    { "Cutoff", "cutoff", 20.0f, 20000.0f, 2500.0f, "Hz", 1.0f },
    { "Resonance", "resonance", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Filter Env", "filter_env", -100.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Key Track", "key_track", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Filter Attack", "filter_attack", 1.0f, 10000.0f, 5.0f, "ms", 1.0f },
    { "Filter Decay", "filter_decay", 1.0f, 10000.0f, 600.0f, "ms", 1.0f },
    { "Filter Sustain", "filter_sustain", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Filter Release", "filter_release", 1.0f, 10000.0f, 400.0f, "ms", 1.0f },
    { "Attack", "attack", 1.0f, 10000.0f, 5.0f, "ms", 1.0f },
    { "Decay", "decay", 1.0f, 10000.0f, 600.0f, "ms", 1.0f },
    { "Sustain", "sustain", 0.0f, 100.0f, 70.0f, "%", 1.0f },
    { "Release", "release", 1.0f, 10000.0f, 300.0f, "ms", 1.0f },
    { "LFO Shape", "lfo_shape", 0.0f, 4.0f, 0.0f, "", 1.0f, kLfoShapeOptions },
    { "LFO Rate", "lfo_rate", 0.05f, 20.0f, 5.0f, "Hz", 0.01f },
    { "LFO Sync", "lfo_sync", 0.0f, 9.0f, 0.0f, "", 1.0f, kLfoSyncOptions },
    { "LFO Pitch", "lfo_pitch", 0.0f, 12.0f, 0.0f, "st", 0.01f },
    { "LFO Filter", "lfo_filter", 0.0f, 4.0f, 0.0f, "oct", 0.1f },
    { "LFO Position", "lfo_position", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Glide", "glide", 0.0f, 2000.0f, 0.0f, "ms", 1.0f },
    { "Voice Mode", "voice_mode", 0.0f, 2.0f, 0.0f, "", 1.0f, kVoiceModeOptions },
    { "Unison", "unison", 1.0f, 4.0f, 1.0f, "", 1.0f },
    { "Spread", "spread", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Character", "character", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Chorus Mix", "chorus_mix", 0.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Delay Mix", "delay_mix", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Delay Time", "delay_time", 0.0f, 7.0f, 3.0f, "", 1.0f, kDelayTimeOptions },
    { "Delay Feedback", "delay_feedback", 0.0f, 90.0f, 35.0f, "%", 1.0f },
    { "Reverb Mix", "reverb_mix", 0.0f, 100.0f, 15.0f, "%", 1.0f },
    { "Reverb Size", "reverb_size", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Velocity", "velocity", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Bend Range", "bend_range", 0.0f, 24.0f, 2.0f, "st", 1.0f },
    { "Octave", "octave", -3.0f, 3.0f, 0.0f, "", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "MPE", "mpe", 0.0f, 1.0f, 0.0f, "", 1.0f, kMpeOptions },
    { "MPE Bend Range", "mpe_bend_range", 0.0f, 96.0f, 48.0f, "st", 1.0f },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nullsynth
