/*
 * NULL JSFX - Rack: parameters, state strings and preset types (framework-free)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Parameters, in order: 22 globals (input, output, mix, the two crossovers, 8 macros, two LFOs,
 * the envelope), then per slot (8): On, Mix, Band, Channel and Value 1..25. Slot values are
 * normalized 0..1 so one parameter serves any module; valueToPlain maps them to the module's
 * slider (log sliders in log space, snapped to the slider's step, enums to integers).
 *
 * State (strings): "slots" = 8 module ids separated by ',' (empty = empty slot), "routes" =
 * "source>slot.value:depth" separated by ';' (slot and value from 1, e.g. "lfo1>2.1:-0.25"),
 * "preset" = the name of the preset the rack was loaded from.
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

enum Band { kBandFull = 0, kBandLow, kBandMid, kBandHigh };
enum Channel { kChanStereo = 0, kChanMid, kChanSide, kChanLeft, kChanRight };

// ---- modulation sources ----------------------------------------------------------------------

enum Source { kSrcMacro1 = 0, kSrcLfo1 = 8, kSrcLfo2 = 9, kSrcEnv = 10, kNumSources = 11 };
static const char* const kSourceSymbols[] = { "macro1", "macro2", "macro3", "macro4", "macro5", "macro6",
                                              "macro7", "macro8", "lfo1", "lfo2", "env" };
static const char* const kSourceNames[] = { "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Macro 5", "Macro 6",
                                            "Macro 7", "Macro 8", "LFO 1", "LFO 2", "Envelope" };

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

static constexpr uint32_t kNumParams = kNumGlobals + kSlots * kSlotParams;

inline constexpr uint32_t slotParam(uint32_t slot, uint32_t p) { return kNumGlobals + slot * kSlotParams + p; }
inline constexpr uint32_t valueParam(uint32_t slot, uint32_t v) { return slotParam(slot, kSlotValue1 + v); }

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

// name, symbol, range, default, unit, step and options of any parameter (slot values are 0..1);
// name and symbol are written to the given buffers for slot parameters
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

inline int globalFromSymbol(const char* sym)
{
    for (uint32_t i = 0; i < kNumGlobals; ++i)
        if (std::strcmp(sym, kGlobals[i].s.symbol) == 0)
            return int(i);
    return -1;
}

// ---- slider mapping --------------------------------------------------------------------------

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// the slider's value snapped to its step (in double, so 0.1 steps land on the JSFX grid)
inline float snapToStep(const DISTRHO::SliderInfo& in, double v)
{
    if (v < in.min) v = in.min;
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
    int slot = 0;     // 0..7
    int value = 0;    // 0..24
    float depth = 0.0f;
};

inline std::string formatRoutes(const Route* r, uint32_t n)
{
    std::string s;
    char buf[64];
    for (uint32_t i = 0; i < n; ++i)
    {
        if (r[i].source < 0 || r[i].source >= kNumSources)
            continue;
        std::snprintf(buf, sizeof(buf), "%s%s>%d.%d:%.4f", s.empty() ? "" : ";", kSourceSymbols[r[i].source],
                      r[i].slot + 1, r[i].value + 1, double(r[i].depth));
        s += buf;
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
                float depth = 0.0f;
                const int src = sourceFromSymbol(buf);
                if (src >= 0 && std::sscanf(gt + 1, "%d.%d:%f", &slot, &value, &depth) == 3 && slot >= 1 &&
                    slot <= int(kSlots) && value >= 1 && value <= int(kValues))
                {
                    out[n].source = src;
                    out[n].slot = slot - 1;
                    out[n].value = value - 1;
                    out[n].depth = depth < -1.0f ? -1.0f : (depth > 1.0f ? 1.0f : depth);
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
};

struct PresetRoute {
    const char* source; // kSourceSymbols
    uint8_t slot, value;
    float depth;
    const char* name;   // the target slider's name
};

struct PresetGlobal {
    const char* symbol; // kGlobals symbol
    float value;
    const char* option; // enum option label, or nullptr to use value
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
};

struct LogSlider {
    const char* id;
    uint8_t index;
};

} // namespace rack
