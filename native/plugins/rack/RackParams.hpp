/*
 * NULL JSFX - Rack: parameters, state strings and preset types (framework-free)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Parameters, in order: 22 globals (input, output, mix, the two crossovers, 8 macros, two LFOs,
 * the envelope), then per slot (8): On, Mix, Band, Channel and Value 1..25 (indices 0..253, as in
 * Rack 2.1). Slot values are normalized 0..1 so one parameter serves any module; valueToPlain
 * maps them to the module's slider (log sliders in log space, snapped to the slider's step, enums
 * to integers).
 *
 * Appended after Rack 2.1 (never insert before them: hosts store parameters by index), 254..285:
 * the step sequencer (Steps, Rate, Sync, Glide, Step 1..16), Lane Mix, Feedback, Feedback From,
 * Feedback To, then S1..S8 Lane (A or B). At their defaults the rack sounds exactly as 2.1: no
 * slot on lane B, feedback 0, and the sequencer moves nothing without a route.
 *
 * Appended in 4.0, 286..309: S1..S8 Lane C/D ("A/B" leaves the slot on its Lane, "C" or "D" moves
 * it to lane C or D), then Gain, Pan, Mute and Solo of lanes A..D. At their defaults nothing moves.
 * Then 310..332: S1..S8 Split Band ("Off" leaves the slot on its Band; 1..5 puts it on that band of
 * the rack's own split into up to 5 bands), Split Bands (2..5), Split 1..4 (the crossovers) and
 * Solo and Bypass of bands 1..5. With no slot on a split band nothing moves.
 *
 * State (strings): "slots" = 8 module ids separated by ',' (empty = empty slot), "routes" =
 * "source>slot.value:depth" separated by ';' (slot and value from 1, e.g. "lfo1>2.1:-0.25";
 * sources: kSourceSymbols, "seq" added after 2.1), "preset" = the name of the preset the rack was
 * loaded from.
 */

#pragma once

#include "NullModule.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rack {

static constexpr uint32_t kSlots = 8;
static constexpr uint32_t kValues = 25;
static constexpr uint32_t kMaxRoutes = 32;

// ---- options (build_chains.py reads kBandOptions and kChannelOptions) -----------------------

static const char* const kOffOn[] = { "Off", "On" };
static const char* const kBandOptions[] = { "Full", "Low", "Mid", "High" };
static const char* const kChannelOptions[] = { "Stereo", "Mid", "Side", "Left", "Right" };
static const char* const kSyncOptions[] = { "Off", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                            "1/2T", "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D" };
// beats (quarter notes) per LFO cycle for each kSyncOptions entry (0 = free)
static const double kSyncBeats[] = { 0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                     4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 3.0, 1.5, 0.75 };
static const char* const kShapeOptions[] = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" };
static const char* const kLaneOptions[] = { "A", "B" };
static const char* const kLaneXOptions[] = { "A/B", "C", "D" }; // added in 4.0: lanes C and D
static constexpr uint32_t kLanes = 4;
static const char* const kSplitOptions[] = { "Off", "1", "2", "3", "4", "5" }; // added in 4.0
static constexpr uint32_t kMaxBands = 5;

enum Band { kBandFull = 0, kBandLow, kBandMid, kBandHigh };
enum Lane { kLaneA = 0, kLaneB, kLaneC, kLaneD };
enum Channel { kChanStereo = 0, kChanMid, kChanSide, kChanLeft, kChanRight };

// ---- modulation sources ----------------------------------------------------------------------

// 4.0 appends Smooth Random, the follower of a slot's output ("sidechain envelope"), the pitch
// follower and the ADSR, then the MIDI sources: the last held note (0..127 as 0..1), its velocity,
// the gate (1 while a note is held), the mod wheel, pressure, pitch bend (-1..1) and one CC
enum Source { kSrcMacro1 = 0, kSrcLfo1 = 8, kSrcLfo2 = 9, kSrcEnv = 10, kSrcSeq = 11,
              kSrcRand = 12, kSrcFollow = 13, kSrcPitch = 14, kSrcAdsr = 15,
              kSrcNote = 16, kSrcVelocity, kSrcGate, kSrcModWheel, kSrcPressure, kSrcBend, kSrcCC,
              kNumSources };
static const char* const kSourceSymbols[] = { "macro1", "macro2", "macro3", "macro4", "macro5", "macro6",
                                              "macro7", "macro8", "lfo1", "lfo2", "env", "seq",
                                              "rand", "follow", "pitch", "adsr",
                                              "note", "velocity", "gate", "modwheel", "pressure", "bend", "cc" };
static const char* const kSourceNames[] = { "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6",
                                            "Macro 7", "Macro 8", "LFO 1", "LFO 2", "Envelope", "Step Seq",
                                            "Random", "Follower", "Pitch", "ADSR",
                                            "Note", "Velocity", "Gate", "Mod Wheel", "Pressure", "Pitch Bend",
                                            "MIDI CC" };

inline int sourceFromSymbol(const char* s)
{
    for (int i = 0; i < kNumSources; ++i)
        if (std::strcmp(s, kSourceSymbols[i]) == 0)
            return i;
    return -1;
}

// ---- parameters ------------------------------------------------------------------------------

enum Param {
    kInput = 0, kOutput, kMix, kXoverLow, kXoverHigh,
    kMacro1, // .. kMacro1 + 7
    kLfo1Rate = kMacro1 + 8, kLfo1Sync, kLfo1Shape,
    kLfo2Rate, kLfo2Sync, kLfo2Shape,
    kEnvAttack, kEnvRelease, kEnvGain,
    kNumGlobals,
};

enum SlotParam { kSlotOn = 0, kSlotMix, kSlotBand, kSlotChannel, kSlotValue1, kSlotParams = kSlotValue1 + kValues };

static constexpr uint32_t kSeqSteps = 16;

// appended after every 2.1 parameter
enum ExtParam : uint32_t {
    kExtFirst = kNumGlobals + kSlots * kSlotParams, // 254
    kSeqLength = kExtFirst, kSeqRate, kSeqSync, kSeqGlide,
    kSeqStep1,                                       // .. kSeqStep1 + 15
    kLaneMix = kSeqStep1 + kSeqSteps,
    kFeedback, kFbFrom, kFbTo,
    kSlotLane1,                                      // .. kSlotLane1 + 7
    // appended in 4.0
    kSlotLaneX1 = kSlotLane1 + kSlots,               // 286 .. 293: S1..S8 Lane C/D
    kLaneGain1 = kSlotLaneX1 + kSlots,               // 294 .. 297: Lane A..D Gain
    kLanePan1 = kLaneGain1 + kLanes,                 // 298 .. 301
    kLaneMute1 = kLanePan1 + kLanes,                 // 302 .. 305
    kLaneSolo1 = kLaneMute1 + kLanes,                // 306 .. 309
    kSlotSplit1 = kLaneSolo1 + kLanes,               // 310 .. 317: S1..S8 Split Band
    kSplitBands = kSlotSplit1 + kSlots,              // 318
    kSplitFreq1,                                     // 319 .. 322: Split 1..4
    kBandSolo1 = kSplitFreq1 + kMaxBands - 1,        // 323 .. 327: Band 1..5 Solo
    kBandBypass1 = kBandSolo1 + kMaxBands,           // 328 .. 332: Band 1..5 Bypass
    // the 4.0 modulators
    kRandRate = kBandBypass1 + kMaxBands,            // 333
    kRandSmooth,
    kFollowSlot, kFollowAttack, kFollowRelease, kFollowGain, // 335 .. 338
    kPitchLow, kPitchHigh,                           // 339, 340
    kAdsrAttack, kAdsrDecay, kAdsrSustain, kAdsrRelease, kAdsrThreshold, // 341 .. 345
    // the 4.0 scenes
    kSceneMorph,                                     // 346
    kScenesOn,                                       // 347
    // the 4.0 MIDI input
    kMidiChannel,                                    // 348: Omni, 1..16
    kMidiCC,                                         // 349: the CC of the source "cc"
    kAdsrTrigger,                                    // 350: Audio, MIDI, Both
    kNumParams,                                      // 351
};
static constexpr float kMaxFeedback = 95.0f;

inline constexpr uint32_t slotParam(uint32_t slot, uint32_t p) { return kNumGlobals + slot * kSlotParams + p; }
inline constexpr uint32_t valueParam(uint32_t slot, uint32_t v) { return slotParam(slot, kSlotValue1 + v); }
inline constexpr uint32_t laneParam(uint32_t slot) { return kSlotLane1 + slot; }
inline constexpr uint32_t laneXParam(uint32_t slot) { return kSlotLaneX1 + slot; }
inline constexpr uint32_t splitParam(uint32_t slot) { return kSlotSplit1 + slot; }

// the lane a slot runs on (kLaneA..kLaneD) from its Lane and Lane C/D parameters
inline int slotLane(float lane, float laneX)
{
    const int x = int(std::lround(laneX));
    return x == 1 ? kLaneC : x == 2 ? kLaneD : (lane >= 0.5f ? kLaneB : kLaneA);
}

struct ParamInfo {
    DISTRHO::SliderInfo s;
    bool log; // knob on a log scale
};

static const ParamInfo kGlobals[kNumGlobals] = {
    { { "Input", "input", -24.0f, 24.0f, 0.0f, "dB", 0.1f }, false },
    { { "Output", "output", -24.0f, 24.0f, 0.0f, "dB", 0.1f }, false },
    { { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f }, false },
    { { "Crossover Low/Mid", "xover_low", 40.0f, 2000.0f, 200.0f, "Hz", 1.0f }, true },
    { { "Crossover Mid/High", "xover_high", 400.0f, 16000.0f, 2500.0f, "Hz", 1.0f }, true },
    { { "Macro 1", "macro1", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Macro 2", "macro2", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Macro 3", "macro3", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Macro 4", "macro4", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Macro 5", "macro5", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Macro 6", "macro6", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Macro 7", "macro7", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Macro 8", "macro8", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "LFO 1 Rate", "lfo1_rate", 0.02f, 20.0f, 1.0f, "Hz", 0.0f }, true },
    { { "LFO 1 Sync", "lfo1_sync", 0.0f, 15.0f, 0.0f, "", 1.0f, kSyncOptions }, false },
    { { "LFO 1 Shape", "lfo1_shape", 0.0f, 5.0f, 0.0f, "", 1.0f, kShapeOptions }, false },
    { { "LFO 2 Rate", "lfo2_rate", 0.02f, 20.0f, 0.25f, "Hz", 0.0f }, true },
    { { "LFO 2 Sync", "lfo2_sync", 0.0f, 15.0f, 0.0f, "", 1.0f, kSyncOptions }, false },
    { { "LFO 2 Shape", "lfo2_shape", 0.0f, 5.0f, 1.0f, "", 1.0f, kShapeOptions }, false },
    { { "Env Attack", "env_attack", 0.1f, 500.0f, 10.0f, "ms", 0.0f }, true },
    { { "Env Release", "env_release", 1.0f, 2000.0f, 150.0f, "ms", 0.0f }, true },
    { { "Env Gain", "env_gain", -12.0f, 36.0f, 6.0f, "dB", 0.1f }, false },
};

static const DISTRHO::SliderInfo kSlotInfos[4] = {
    { "On", "on", 0.0f, 1.0f, 1.0f, "", 1.0f, kOffOn },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Band", "band", 0.0f, 3.0f, 0.0f, "", 1.0f, kBandOptions },
    { "Channel", "channel", 0.0f, 4.0f, 0.0f, "", 1.0f, kChannelOptions },
};

// the step sequencer's default pattern (Step 1..16)
static const float kSeqDefaults[kSeqSteps] = { 1.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.0f, 0.5f, 0.25f,
                                               1.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.25f, 0.5f, 0.0f };

static const ParamInfo kExtInfos[] = {
    { { "Seq Steps", "seq_steps", 1.0f, 16.0f, 16.0f, "", 1.0f }, false },
    { { "Seq Rate", "seq_rate", 0.1f, 40.0f, 4.0f, "Hz", 0.0f }, true },
    { { "Seq Sync", "seq_sync", 0.0f, 15.0f, 7.0f, "", 1.0f, kSyncOptions }, false },
    { { "Seq Glide", "seq_glide", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Seq Step", "seq_step", 0.0f, 1.0f, 0.0f, "", 0.0f }, false },     // x16, numbered
    { { "Lane Mix", "lane_mix", 0.0f, 100.0f, 50.0f, "%", 0.0f }, false },
    { { "Feedback", "feedback", 0.0f, kMaxFeedback, 0.0f, "%", 1.0f }, false },
    { { "Feedback From", "fb_from", 1.0f, 8.0f, 1.0f, "", 1.0f }, false },
    { { "Feedback To", "fb_to", 1.0f, 8.0f, 8.0f, "", 1.0f }, false },
    { { "Lane", "lane", 0.0f, 1.0f, 0.0f, "", 1.0f, kLaneOptions }, false }, // x8, per slot
    // appended in 4.0
    { { "Lane C/D", "lanex", 0.0f, 2.0f, 0.0f, "", 1.0f, kLaneXOptions }, false }, // x8, per slot
    { { "Gain", "gain", -24.0f, 12.0f, 0.0f, "dB", 0.1f }, false },                 // x4, per lane
    { { "Pan", "pan", -100.0f, 100.0f, 0.0f, "%", 1.0f }, false },
    { { "Mute", "mute", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn }, false },
    { { "Solo", "solo", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn }, false },
    { { "Split Band", "split", 0.0f, 5.0f, 0.0f, "", 1.0f, kSplitOptions }, false }, // 15: x8, per slot
    { { "Split Bands", "split_bands", 2.0f, 5.0f, 5.0f, "", 1.0f }, false },
    { { "Split", "split_f", 20.0f, 20000.0f, 100.0f, "Hz", 1.0f }, true },          // 17: x4, numbered
    { { "Solo", "solo", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn }, false },               // 18: x5, per band
    { { "Bypass", "bypass", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn }, false },           // 19: x5, per band
};
static const char* const kMidiChannels[] = { "Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
                                             "12", "13", "14", "15", "16" };
static const char* const kAdsrTriggers[] = { "Audio", "MIDI", "Both" };
// the 4.0 modulators' settings (kRandRate .. kAdsrTrigger, in order)
static const ParamInfo kModInfos[] = {
    { { "Random Rate", "rand_rate", 0.02f, 20.0f, 1.0f, "Hz", 0.0f }, true },
    { { "Random Smooth", "rand_smooth", 0.0f, 100.0f, 100.0f, "%", 1.0f }, false },
    { { "Follower Slot", "follow_slot", 1.0f, 8.0f, 8.0f, "", 1.0f }, false },
    { { "Follower Attack", "follow_attack", 0.1f, 500.0f, 10.0f, "ms", 0.0f }, true },
    { { "Follower Release", "follow_release", 1.0f, 2000.0f, 150.0f, "ms", 0.0f }, true },
    { { "Follower Gain", "follow_gain", -12.0f, 36.0f, 6.0f, "dB", 0.1f }, false },
    { { "Pitch Low", "pitch_low", 20.0f, 2000.0f, 80.0f, "Hz", 1.0f }, true },
    { { "Pitch High", "pitch_high", 100.0f, 8000.0f, 1000.0f, "Hz", 1.0f }, true },
    { { "ADSR Attack", "adsr_attack", 0.1f, 5000.0f, 10.0f, "ms", 0.0f }, true },
    { { "ADSR Decay", "adsr_decay", 1.0f, 5000.0f, 200.0f, "ms", 0.0f }, true },
    { { "ADSR Sustain", "adsr_sustain", 0.0f, 100.0f, 60.0f, "%", 1.0f }, false },
    { { "ADSR Release", "adsr_release", 1.0f, 5000.0f, 300.0f, "ms", 0.0f }, true },
    { { "ADSR Threshold", "adsr_threshold", -60.0f, 0.0f, -30.0f, "dB", 0.1f }, false },
    { { "Scene Morph", "scene_morph", 0.0f, 100.0f, 0.0f, "%", 0.0f }, false },
    { { "Scenes", "scenes", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn }, false },
    { { "MIDI Channel", "midi_channel", 0.0f, 16.0f, 0.0f, "", 1.0f, kMidiChannels }, false },
    { { "MIDI CC", "midi_cc", 0.0f, 127.0f, 74.0f, "", 1.0f }, false },
    { { "ADSR Trigger", "adsr_trigger", 0.0f, 2.0f, 0.0f, "", 1.0f, kAdsrTriggers }, false },
};

// the modulator settings a route can move (4.0): a route to slot "m" moves the setting
// kModTargets[value]; its depth is a share of the setting's range (log ones in log space)
static const uint32_t kModTargets[] = { kLfo1Rate, kLfo2Rate, kEnvGain, kSeqRate, kRandRate, kRandSmooth,
                                        kAdsrAttack, kAdsrDecay, kAdsrSustain, kAdsrRelease,
                                        kSceneMorph }; // Scene Morph appended with the MIDI input
static const char* const kModTargetNames[] = { "LFO 1 Rate", "LFO 2 Rate", "Env Gain", "Seq Rate", "Random Rate",
                                               "Random Smooth", "ADSR Attack", "ADSR Decay", "ADSR Sustain",
                                               "ADSR Release", "Scene Morph" };
static constexpr uint32_t kNumModTargets = sizeof(kModTargets) / sizeof(kModTargets[0]);
static constexpr int kModSlot = int(kSlots); // Route::slot of a route to a modulator setting

// the defaults of Split 1..4: two octaves apart
static const float kSplitDefaults[kMaxBands - 1] = { 100.0f, 400.0f, 1600.0f, 6400.0f };

// name, symbol, range, default, unit, step and options of any parameter (slot values are 0..1);
// name and symbol are written to the given buffers for slot, step and appended parameters
inline DISTRHO::SliderInfo paramInfo(uint32_t index, char* name, size_t nn, char* sym, size_t ns, bool* log = nullptr)
{
    if (log)
        *log = false;
    if (index < kNumGlobals)
    {
        if (log)
            *log = kGlobals[index].log;
        return kGlobals[index].s;
    }
    if (index >= kExtFirst)
    {
        DISTRHO::SliderInfo s;
        if (index >= kSeqStep1 && index < kSeqStep1 + kSeqSteps)
        {
            s = kExtInfos[4].s;
            s.def = kSeqDefaults[index - kSeqStep1];
            std::snprintf(name, nn, "Seq Step %u", index - kSeqStep1 + 1);
            std::snprintf(sym, ns, "seq_step%u", index - kSeqStep1 + 1);
        }
        else if (index >= kSlotLane1 && index < kSlotLane1 + kSlots)
        {
            s = kExtInfos[9].s;
            std::snprintf(name, nn, "S%u Lane", index - kSlotLane1 + 1);
            std::snprintf(sym, ns, "s%u_lane", index - kSlotLane1 + 1);
        }
        else if (index >= kSlotLaneX1 && index < kSlotLaneX1 + kSlots)
        {
            s = kExtInfos[10].s;
            std::snprintf(name, nn, "S%u Lane C/D", index - kSlotLaneX1 + 1);
            std::snprintf(sym, ns, "s%u_lanex", index - kSlotLaneX1 + 1);
        }
        else if (index >= kSlotSplit1 && index < kSlotSplit1 + kSlots)
        {
            s = kExtInfos[15].s;
            std::snprintf(name, nn, "S%u Split Band", index - kSlotSplit1 + 1);
            std::snprintf(sym, ns, "s%u_split", index - kSlotSplit1 + 1);
        }
        else if (index == kSplitBands)
        {
            s = kExtInfos[16].s;
            std::snprintf(name, nn, "%s", s.name);
            std::snprintf(sym, ns, "%s", s.symbol);
        }
        else if (index >= kSplitFreq1 && index < kBandSolo1)
        {
            if (log)
                *log = true;
            s = kExtInfos[17].s;
            s.def = kSplitDefaults[index - kSplitFreq1];
            std::snprintf(name, nn, "Split %u", index - kSplitFreq1 + 1);
            std::snprintf(sym, ns, "split_f%u", index - kSplitFreq1 + 1);
        }
        else if (index >= kRandRate && index < kNumParams)
        {
            if (log)
                *log = kModInfos[index - kRandRate].log;
            s = kModInfos[index - kRandRate].s;
            std::snprintf(name, nn, "%s", s.name);
            std::snprintf(sym, ns, "%s", s.symbol);
        }
        else if (index >= kBandSolo1 && index < kRandRate)
        {
            // Band 1..5 Solo, Bypass
            const uint32_t k = (index - kBandSolo1) / kMaxBands, band = (index - kBandSolo1) % kMaxBands;
            s = kExtInfos[18 + k].s;
            std::snprintf(name, nn, "Band %u %s", band + 1, s.name);
            std::snprintf(sym, ns, "band%u_%s", band + 1, s.symbol);
        }
        else if (index >= kLaneGain1 && index < kSlotSplit1)
        {
            // Lane A..D Gain, Pan, Mute, Solo
            const uint32_t k = (index - kLaneGain1) / kLanes, lane = (index - kLaneGain1) % kLanes;
            s = kExtInfos[11 + k].s;
            std::snprintf(name, nn, "Lane %c %s", char('A' + lane), s.name);
            std::snprintf(sym, ns, "lane%c_%s", char('a' + lane), s.symbol);
        }
        else
        {
            // Steps, Rate, Sync, Glide (0..3); Lane Mix, Feedback, From, To (5..8)
            const uint32_t k = index < kSeqStep1 ? index - kExtFirst : index - kLaneMix + 5;
            if (k > 8)
                return { "", "", 0.0f, 1.0f, 0.0f, "", 0.0f };
            if (log)
                *log = kExtInfos[k].log;
            s = kExtInfos[k].s;
            std::snprintf(name, nn, "%s", s.name);
            std::snprintf(sym, ns, "%s", s.symbol);
        }
        s.name = name;
        s.symbol = sym;
        return s;
    }
    const uint32_t k = index - kNumGlobals, slot = k / kSlotParams, p = k % kSlotParams;
    DISTRHO::SliderInfo s;
    if (p < kSlotValue1)
    {
        s = kSlotInfos[p];
        std::snprintf(name, nn, "S%u %s", slot + 1, kSlotInfos[p].name);
        std::snprintf(sym, ns, "s%u_%s", slot + 1, kSlotInfos[p].symbol);
    }
    else
    {
        s = { "", "", 0.0f, 1.0f, 0.0f, "", 0.0f };
        std::snprintf(name, nn, "S%u Value %u", slot + 1, p - kSlotValue1 + 1);
        std::snprintf(sym, ns, "s%u_v%u", slot + 1, p - kSlotValue1 + 1);
    }
    s.name = name;
    s.symbol = sym;
    return s;
}

// true for the per-slot parameters: On, Mix, Band, Channel, the values and (added after 2.1) the lane
// and (4.0) the lane C/D and the split band
inline bool isSlotParam(uint32_t index, uint32_t* slot = nullptr)
{
    if (index >= kNumGlobals && index < kExtFirst)
    {
        if (slot)
            *slot = (index - kNumGlobals) / kSlotParams;
        return true;
    }
    if (index >= kSlotLane1 && index < kSlotLane1 + kSlots)
    {
        if (slot)
            *slot = index - kSlotLane1;
        return true;
    }
    if (index >= kSlotLaneX1 && index < kSlotLaneX1 + kSlots)
    {
        if (slot)
            *slot = index - kSlotLaneX1;
        return true;
    }
    if (index >= kSlotSplit1 && index < kSlotSplit1 + kSlots)
    {
        if (slot)
            *slot = index - kSlotSplit1;
        return true;
    }
    return false;
}

// the index of a global parameter (one of kGlobals, or an appended one other than a slot's
// lane) by its symbol, -1 if there is none
inline int globalFromSymbol(const char* sym)
{
    for (uint32_t i = 0; i < kNumGlobals; ++i)
        if (std::strcmp(sym, kGlobals[i].s.symbol) == 0)
            return int(i);
    for (uint32_t i = kExtFirst; i < kNumParams; ++i)
    {
        if (isSlotParam(i))
            continue;
        char n[32], s[32];
        paramInfo(i, n, sizeof(n), s, sizeof(s));
        if (std::strcmp(sym, s) == 0)
            return int(i);
    }
    return -1;
}

// ---- slider mapping --------------------------------------------------------------------------

// NaN becomes 0
inline float clamp01(float v) { return v > 0.0f ? (v < 1.0f ? v : 1.0f) : 0.0f; }
// NaN becomes lo
inline float clamp(float v, float lo, float hi) { return v > lo ? (v < hi ? v : hi) : lo; }

// the slider's value snapped to its step (in double, so 0.1 steps land on the JSFX grid)
inline float snapToStep(const DISTRHO::SliderInfo& in, double v)
{
    if (!(v >= in.min)) v = in.min; // also NaN
    if (v > in.max) v = in.max;
    double st = in.step;
    if (in.options && st < 1.0)
        st = 1.0;
    if (st > 0.0)
    {
        v = double(in.min) + std::round((v - double(in.min)) / st) * st;
        if (v > in.max) v = in.max;
        if (v < in.min) v = in.min;
    }
    return float(v);
}

inline float valueToPlain(const DISTRHO::SliderInfo& in, bool log, float norm)
{
    const double t = clamp01(norm);
    double v;
    if (log && in.min > 0.0f)
        v = double(in.min) * std::exp(t * std::log(double(in.max) / double(in.min)));
    else
        v = double(in.min) + t * (double(in.max) - double(in.min));
    return snapToStep(in, v);
}

inline float plainToValue(const DISTRHO::SliderInfo& in, bool log, float v)
{
    if (in.max <= in.min)
        return 0.0f;
    if (log && in.min > 0.0f)
        return clamp01(float(std::log(double(v < in.min ? in.min : v) / in.min) / std::log(double(in.max) / in.min)));
    return clamp01(float((double(v) - in.min) / (double(in.max) - in.min)));
}

// ---- routes ----------------------------------------------------------------------------------

struct Route {
    int source = -1;  // Source, -1 = unused
    int slot = 0;     // 0..7; 4.0: kModSlot = a modulator setting
    int value = 0;    // 0..24; for kModSlot 0..kNumModTargets-1
    float depth = 0.0f;
    float curve = 0.0f; // 4.0: Remap, -1..1 (0 = straight; > 0 bends the source up, < 0 down)
};

// Remap (4.0): the source value through the route's curve, a power law on |v| that keeps the sign
// (curve 1: |v|^0.25, -1: |v|^4)
inline float remap(float v, float curve)
{
    if (curve == 0.0f)
        return v;
    const double a = std::fabs(double(v)), y = std::pow(a, std::pow(4.0, -double(curve)));
    return float(v < 0.0f ? -y : y);
}

inline std::string formatRoutes(const Route* r, uint32_t n)
{
    std::string s;
    char buf[64];
    for (uint32_t i = 0; i < n; ++i)
    {
        if (r[i].source < 0 || r[i].source >= kNumSources)
            continue;
        if (r[i].slot == kModSlot)
            std::snprintf(buf, sizeof(buf), "%s%s>m.%d:%.4f", s.empty() ? "" : ";", kSourceSymbols[r[i].source],
                          r[i].value + 1, double(r[i].depth));
        else
            std::snprintf(buf, sizeof(buf), "%s%s>%d.%d:%.4f", s.empty() ? "" : ";", kSourceSymbols[r[i].source],
                          r[i].slot + 1, r[i].value + 1, double(r[i].depth));
        s += buf;
        if (r[i].curve != 0.0f)
        {
            // 4.0: the Remap curve as a suffix, only when bent (a 2.x string stays as it was)
            std::snprintf(buf, sizeof(buf), "~%.4f", double(r[i].curve));
            s += buf;
        }
    }
    return s;
}

// parses up to kMaxRoutes routes; malformed entries are skipped
inline uint32_t parseRoutes(const char* s, Route* out)
{
    uint32_t n = 0;
    while (s && *s && n < kMaxRoutes)
    {
        const char* end = std::strchr(s, ';');
        const size_t len = end ? size_t(end - s) : std::strlen(s);
        char buf[64];
        if (len < sizeof(buf))
        {
            std::memcpy(buf, s, len);
            buf[len] = '\0';
            char* gt = std::strchr(buf, '>');
            if (gt)
            {
                *gt = '\0';
                int slot = 0, value = 0;
                float depth = 0.0f, curve = 0.0f;
                const int src = sourceFromSymbol(buf);
                char* tilde = std::strchr(gt + 1, '~');
                bool ok = true;
                if (tilde)
                {
                    *tilde = '\0';
                    char* e = nullptr;
                    curve = std::strtof(tilde + 1, &e);
                    ok = e != tilde + 1 && *e == '\0' && std::isfinite(curve);
                }
                bool parsed = false;
                if (gt[1] == 'm' && gt[2] == '.')
                {
                    parsed = std::sscanf(gt + 3, "%d:%f", &value, &depth) == 2 && value >= 1 && value <= int(kNumModTargets);
                    slot = kModSlot + 1;
                }
                else
                    parsed = std::sscanf(gt + 1, "%d.%d:%f", &slot, &value, &depth) == 3 && slot >= 1 &&
                             slot <= int(kSlots) && value >= 1 && value <= int(kValues);
                if (src >= 0 && ok && parsed && std::isfinite(depth))
                {
                    out[n].source = src;
                    out[n].slot = slot - 1;
                    out[n].value = value - 1;
                    out[n].depth = depth < -1.0f ? -1.0f : (depth > 1.0f ? 1.0f : depth);
                    out[n].curve = curve < -1.0f ? -1.0f : (curve > 1.0f ? 1.0f : curve);
                    ++n;
                }
            }
        }
        s = end ? end + 1 : nullptr;
    }
    return n;
}

// "a,b,,c,..." -> 8 ids (missing ones empty)
inline void parseSlots(const char* s, std::string ids[kSlots])
{
    for (uint32_t i = 0; i < kSlots; ++i)
        ids[i].clear();
    uint32_t i = 0;
    for (const char* p = s; p && *p && i < kSlots; ++p)
    {
        if (*p == ',')
            ++i;
        else if (*p != ' ')
            ids[i] += *p;
    }
}

inline std::string formatSlots(const std::string ids[kSlots])
{
    std::string s;
    for (uint32_t i = 0; i < kSlots; ++i)
    {
        if (i)
            s += ',';
        s += ids[i];
    }
    return s;
}

// ---- scenes (4.0) ----------------------------------------------------------------------------

// A scene is a snapshot of every slot's module id, Mix and values and of the 8 macros. With
// Scenes on, Scene Morph (0..100 %) moves through the stored scenes in order and their values
// take the place of the parameters (a slot whose module differs from the scene's keeps its own).
// State "scenes": the stored ones separated by '|', each "n/ids/values/macros" with n from 1, ids
// as "slots", values the 8 x (Mix + 25 values) and macros 8, comma separated ("%.6g").
static constexpr uint32_t kScenes = 4;
static constexpr uint32_t kSceneSlotVals = 1 + kValues; // Mix, then the values

struct Scene {
    bool used = false;
    std::string ids[kSlots];
    float vals[kSlots][kSceneSlotVals] = {};
    float macros[8] = {};
};

inline std::string formatScenes(const Scene* sc)
{
    std::string s;
    char buf[32];
    for (uint32_t k = 0; k < kScenes; ++k)
    {
        if (!sc[k].used)
            continue;
        if (!s.empty())
            s += '|';
        std::snprintf(buf, sizeof(buf), "%u/", k + 1);
        s += buf;
        s += formatSlots(sc[k].ids);
        s += '/';
        for (uint32_t i = 0; i < kSlots; ++i)
            for (uint32_t v = 0; v < kSceneSlotVals; ++v)
            {
                std::snprintf(buf, sizeof(buf), i || v ? ",%.6g" : "%.6g", double(sc[k].vals[i][v]));
                s += buf;
            }
        s += '/';
        for (uint32_t m = 0; m < 8; ++m)
        {
            std::snprintf(buf, sizeof(buf), m ? ",%.6g" : "%.6g", double(sc[k].macros[m]));
            s += buf;
        }
    }
    return s;
}

// a comma separated list of exactly n finite numbers
inline bool parseFloats(const std::string& t, float* out, uint32_t n)
{
    const char* p = t.c_str();
    for (uint32_t i = 0; i < n; ++i)
    {
        char* e = nullptr;
        const float v = std::strtof(p, &e);
        if (e == p || !std::isfinite(v))
            return false;
        out[i] = v;
        p = e;
        if (i + 1 < n)
        {
            if (*p != ',')
                return false;
            ++p;
        }
    }
    return *p == '\0';
}

// parses the stored scenes into sc (all reset first); malformed entries are skipped
inline void parseScenes(const char* s, Scene* sc)
{
    for (uint32_t k = 0; k < kScenes; ++k)
        sc[k] = Scene();
    const std::string all = s ? s : "";
    size_t pos = 0;
    while (pos < all.size())
    {
        size_t end = all.find('|', pos);
        if (end == std::string::npos)
            end = all.size();
        const std::string e = all.substr(pos, end - pos);
        pos = end + 1;
        const size_t a = e.find('/'), b = a == std::string::npos ? a : e.find('/', a + 1),
                     c = b == std::string::npos ? b : e.find('/', b + 1);
        if (c == std::string::npos || e.find('/', c + 1) != std::string::npos)
            continue;
        const int k = std::atoi(e.substr(0, a).c_str());
        if (k < 1 || k > int(kScenes) || e.substr(0, a) != std::to_string(k))
            continue;
        Scene t;
        parseSlots(e.substr(a + 1, b - a - 1).c_str(), t.ids);
        if (!parseFloats(e.substr(b + 1, c - b - 1), &t.vals[0][0], kSlots * kSceneSlotVals) ||
            !parseFloats(e.substr(c + 1), t.macros, 8))
            continue;
        t.used = true;
        sc[k - 1] = t;
    }
}

// ---- presets (data in RackPresets.hpp, generated by tools/build_chains.py) -------------------

struct PresetValue {
    uint8_t index;    // slider index (JSFX order)
    float value;      // plain value (enums: the option index)
    const char* name; // the JSFX slider name, checked against the module's SliderInfo by rack_test
};

struct PresetSlot {
    const char* id;
    const PresetValue* values;
    uint8_t numValues;
    uint8_t band, channel;
    float mix;
    bool on;
    uint8_t lane; // added after 2.1: 0 = A (the generator leaves it out), 1 = B; 4.0: 2 = C, 3 = D
    uint8_t split; // 4.0: the split band, 0 = off (the generator leaves it out), 1..5
};

struct PresetRoute {
    const char* source; // kSourceSymbols
    uint8_t slot, value;
    float depth;
    const char* name;   // the target slider's name
    float curve;        // 4.0: Remap (0 = straight, the generator leaves it out); slot kModSlot = a modulator setting
};

struct PresetGlobal {
    const char* symbol; // a global's symbol (globalFromSymbol: kGlobals or an appended one)
    float value;
    const char* option; // enum option label, or nullptr to use value
};

// 4.0: a preset's scene is the preset itself with some values changed: in scene `scene` (0..3),
// slot `slot` (0..7) gets `plain` for its slider `value` (from 0), or for its Mix with value -1;
// slot kSlots is a macro (value 0..7)
struct PresetSceneValue {
    uint8_t scene, slot;
    int16_t value;
    float plain;
    const char* name; // the slider's name, or "Mix" or "macroN"
};

struct Preset {
    const char* name;
    const char* category; // "Init", "Chain", "Routing" or "Macro"
    const PresetSlot* slots;
    uint32_t numSlots;
    const PresetRoute* routes;
    uint32_t numRoutes;
    const PresetGlobal* globals;
    uint32_t numGlobals;
    // 4.0: the scenes (0 = none, the generator leaves them out) and their changed values
    uint32_t numScenes = 0;
    const PresetSceneValue* sceneValues = nullptr;
    uint32_t numSceneValues = 0;
};

struct LogSlider {
    const char* id;
    uint8_t index;
};

} // namespace rack
