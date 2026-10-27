/*
 * NULL JSFX - Drum Machine: the JSFX sliders, shared by the plugin and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullSliders.hpp"

namespace nulldrum {

static const char* const kPatternOptions[] = { "Off", "User", "House", "Techno", "Boom Bap", "Electro", "Breakbeat", "Trap", "Disco", "Synth Pop", "Dembow", "Bossa",
                                                "Rock", "Funk", "UK Garage", "Jungle", "Afrobeat", "Reggae", "Footwork", "Industrial" };
static const char* const kPatternShort[] = { "OFF", "USER", "HOUSE", "TECHNO", "BOOM BAP", "ELECTRO", "BREAKBEAT", "TRAP", "DISCO", "SYNTH POP", "DEMBOW", "BOSSA",
                                              "ROCK", "FUNK", "UK GARAGE", "JUNGLE", "AFROBEAT", "REGGAE", "FOOTWORK", "INDUSTRIAL" };
static const char* const kGenerateOptions[] = { "Off", "On" };
static const char* const kGenerateShort[] = { "OFF", "ON" };
static const char* const kStyleOptions[] = { "Four Floor", "Breakbeat", "Half Time", "Electro", "Latin", "Random" };
static const char* const kStyleShort[] = { "FOUR FLOOR", "BREAKBEAT", "HALF TIME", "ELECTRO", "LATIN", "RANDOM" };
static const char* const kKitOptions[] = { "Manual", "Deep Boom", "Punch Box", "Vintage", "Pop Studio", "Lo-Fi Dust", "Industrial", "Minimal", "Big Room" };
static const char* const kKitShort[] = { "MANUAL", "DEEP BOOM", "PUNCH BOX", "VINTAGE", "POP STUDIO", "LO-FI DUST", "INDUSTRIAL", "MINIMAL", "BIG ROOM" };
static const char* const kOutputsOptions[] = { "Main", "Groups" };
static const char* const kOutputsShort[] = { "MAIN", "GROUPS" };
static const char* const kSongOptions[] = { "Off", "On" };
static const char* const kSongShort[] = { "OFF", "ON" };

// slider1..slider64 of drum_machine.jsfx, in order
static const DISTRHO::SliderInfo kSliders[] = {
    { "Pattern", "pattern", 0.0f, 19.0f, 2.0f, "", 1.0f, kPatternOptions },
    { "Swing", "swing", 50.0f, 75.0f, 50.0f, "%", 1.0f },
    { "Accent", "accent", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Drive", "drive", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Grit", "grit", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Volume", "volume", -24.0f, 12.0f, 0.0f, "dB", 0.1f },
    { "Kick Level", "kick_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Kick Tune", "kick_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Kick Decay", "kick_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Kick Tone", "kick_tone", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Kick Pan", "kick_pan", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Snare Level", "snare_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Snare Tune", "snare_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Snare Decay", "snare_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Snare Tone", "snare_tone", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Snare Pan", "snare_pan", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Clap Level", "clap_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Clap Tune", "clap_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Clap Decay", "clap_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Clap Tone", "clap_tone", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Clap Pan", "clap_pan", -100.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Rim Level", "rim_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Rim Tune", "rim_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Rim Decay", "rim_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Rim Tone", "rim_tone", 0.0f, 100.0f, 0.0f, "%", 1.0f },
    { "Rim Pan", "rim_pan", -100.0f, 100.0f, -15.0f, "%", 1.0f },
    { "Closed Hat Level", "closed_hat_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Closed Hat Tune", "closed_hat_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Closed Hat Decay", "closed_hat_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Closed Hat Tone", "closed_hat_tone", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Closed Hat Pan", "closed_hat_pan", -100.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Open Hat Level", "open_hat_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Open Hat Tune", "open_hat_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Open Hat Decay", "open_hat_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Open Hat Tone", "open_hat_tone", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Open Hat Pan", "open_hat_pan", -100.0f, 100.0f, 20.0f, "%", 1.0f },
    { "Low Tom Level", "low_tom_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Low Tom Tune", "low_tom_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Low Tom Decay", "low_tom_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Low Tom Tone", "low_tom_tone", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "Low Tom Pan", "low_tom_pan", -100.0f, 100.0f, -25.0f, "%", 1.0f },
    { "High Tom Level", "high_tom_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "High Tom Tune", "high_tom_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "High Tom Decay", "high_tom_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "High Tom Tone", "high_tom_tone", 0.0f, 100.0f, 30.0f, "%", 1.0f },
    { "High Tom Pan", "high_tom_pan", -100.0f, 100.0f, 25.0f, "%", 1.0f },
    { "Cowbell Level", "cowbell_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Cowbell Tune", "cowbell_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Cowbell Decay", "cowbell_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Cowbell Tone", "cowbell_tone", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Cowbell Pan", "cowbell_pan", -100.0f, 100.0f, -10.0f, "%", 1.0f },
    { "Cymbal Level", "cymbal_level", -40.0f, 6.0f, 0.0f, "dB", 0.1f },
    { "Cymbal Tune", "cymbal_tune", -12.0f, 12.0f, 0.0f, "st", 0.1f },
    { "Cymbal Decay", "cymbal_decay", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Cymbal Tone", "cymbal_tone", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Cymbal Pan", "cymbal_pan", -100.0f, 100.0f, 10.0f, "%", 1.0f },
    { "Probability", "probability", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Seed", "seed", 1.0f, 999.0f, 1.0f, "", 1.0f },
    { "Generate", "generate", 0.0f, 1.0f, 0.0f, "", 1.0f, kGenerateOptions },
    { "Density", "density", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Style", "style", 0.0f, 5.0f, 0.0f, "", 1.0f, kStyleOptions },
    { "Kit", "kit", 0.0f, 8.0f, 0.0f, "", 1.0f, kKitOptions },
    { "Outputs", "outputs", 0.0f, 1.0f, 0.0f, "", 1.0f, kOutputsOptions },
    { "Song", "song", 0.0f, 1.0f, 0.0f, "", 1.0f, kSongOptions },
};
static constexpr uint32_t kNumParams = sizeof(kSliders) / sizeof(kSliders[0]);

} // namespace nulldrum
