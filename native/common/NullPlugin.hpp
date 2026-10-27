/*
 * NULL JSFX - base classes for the VST3/CLAP plugins
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * NullOneSliderPlugin: one automatable parameter that mirrors slider1 of the JSFX.
 * NullMultiPlugin: one parameter per JSFX slider, in the order the JSFX declares them.
 * Both carry the shared maker/licence metadata and the Taps the UI reads for its display.
 * A plugin implements getLabel, getDescription, getUniqueId, valueChanged,
 * onActivate/sampleRateChanged and process.
 *
 * Threads: DPF calls setParameterValue from the host's main thread too (VST3 interface edits,
 * VST3 and CLAP state loads), while process() may be running. So setParameterValue only stores
 * the value (atomic) and marks it changed; valueChanged runs on the audio thread at the start
 * of the next block (run), or before onActivate (activate), like the JSFX's @slider.
 */

#pragma once

#include "DistrhoPlugin.hpp"
#include "NullBeatClock.hpp"
#include "NullSliders.hpp"
#include "NullTaps.hpp"

#include <atomic>
#include <cstdint>

// the suite's version, from project(... VERSION) in native/CMakeLists.txt
#if !defined(NULL_JSFX_VERSION_MAJOR) || !defined(NULL_JSFX_VERSION_MINOR) || !defined(NULL_JSFX_VERSION_PATCH)
# error "NULL_JSFX_VERSION_* not defined: build through native/CMakeLists.txt"
#endif

START_NAMESPACE_DISTRHO

// the Taps (meters, scopes) and the metadata every NULL JSFX plugin shares
class NullPluginBase : public Plugin
{
public:
    explicit NullPluginBase(uint32_t parameters, uint32_t states = 0)
        : Plugin(parameters, 0, states) {}

    nulljsfx::Taps taps;

protected:
    const char* getMaker() const override    { return "NULL JSFX"; }
    const char* getHomePage() const override { return "https://nulljsfx.tech"; }
    const char* getLicense() const override  { return "MIT"; }
    uint32_t getVersion() const override
    {
        return d_version(NULL_JSFX_VERSION_MAJOR, NULL_JSFX_VERSION_MINOR, NULL_JSFX_VERSION_PATCH);
    }

    // the plugin's own activate and run; parameter changes are already applied when they start
    virtual void onActivate() {}
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    virtual void process(const float** inputs, float** outputs, uint32_t frames,
                         const MidiEvent* midiEvents, uint32_t midiEventCount) = 0;
#else
    virtual void process(const float** inputs, float** outputs, uint32_t frames) = 0;
#endif

    void activate() final
    {
        applyChangedParameters();
        onActivate();
    }

#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    void run(const float** inputs, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) final
    {
        applyChangedParameters();
        tick(frames);
        process(inputs, outputs, frames, midiEvents, midiEventCount);
    }
#else
    void run(const float** inputs, float** outputs, uint32_t frames) final
    {
        applyChangedParameters();
        tick(frames);
        process(inputs, outputs, frames);
    }
#endif

#if DISTRHO_PLUGIN_WANT_TIMEPOS
    // the host's position for this block, exact in LV2 too (NullBeatClock.hpp); it hides
    // Plugin::getTimePosition for the plugins
    const TimePosition& getTimePosition() const noexcept { return fClock.time(); }
#endif

    // the parameter changes (one bit per index) waiting for the audio thread
    static constexpr uint32_t kMaxChangeBits = 64;

    void markChanged(uint32_t index) noexcept
    {
        fChanged.fetch_or(uint64_t(1) << index, std::memory_order_release);
    }

    // calls applyParameter for every index marked since the last call, lowest index first
    void applyChangedParameters()
    {
        uint64_t bits = fChanged.exchange(0, std::memory_order_acquire);
        for (uint32_t i = 0; bits != 0; ++i, bits >>= 1)
            if (bits & 1)
                applyParameter(i);
    }

    virtual void applyParameter(uint32_t index) = 0;

    static void fillParameter(Parameter& p, const SliderInfo& in)
    {
        p.hints      = kParameterIsAutomatable;
        p.name       = in.name;
        p.shortName  = in.name;
        p.symbol     = in.symbol;
        p.unit       = in.unit;
        p.ranges.min = in.min;
        p.ranges.max = in.max;
        p.ranges.def = in.def;
        if (in.step >= 1.0f)
            p.hints |= kParameterIsInteger;
        if (in.options)
        {
            const int n = int(in.max - in.min + 0.5f) + 1;
            p.hints |= kParameterIsInteger;
            p.enumValues.count = uint8_t(n);
            p.enumValues.restrictedMode = true;
            p.enumValues.values = new ParameterEnumerationValue[n];
            for (int i = 0; i < n; ++i)
            {
                p.enumValues.values[i].value = in.min + float(i);
                p.enumValues.values[i].label = in.options[i];
            }
        }
    }

private:
    void tick(uint32_t frames)
    {
#if DISTRHO_PLUGIN_WANT_TIMEPOS
        fClock.update(Plugin::getTimePosition(), getSampleRate(), frames);
#else
        (void)frames;
#endif
    }

    std::atomic<uint64_t> fChanged { 0 };
#if DISTRHO_PLUGIN_WANT_TIMEPOS
    NullBeatClock fClock;
#endif
};

class NullOneSliderPlugin : public NullPluginBase
{
public:
    explicit NullOneSliderPlugin(const SliderInfo& info)
        : NullPluginBase(1), fInfo(info), fValue(info.def) {}

    float value() const noexcept { return fValue.load(std::memory_order_relaxed); }

protected:
    // called on the audio thread when the parameter has changed (also once from the constructor
    // of the derived class if it wants the default applied; call valueChanged(value()) there)
    virtual void valueChanged(float v) = 0;

    void initParameter(uint32_t index, Parameter& p) override
    {
        if (index == 0)
            fillParameter(p, fInfo);
    }

    float getParameterValue(uint32_t index) const override { return index == 0 ? value() : 0.0f; }

    // any thread
    void setParameterValue(uint32_t index, float v) override
    {
        if (index != 0)
            return;
        fValue.store(v, std::memory_order_relaxed);
        markChanged(0);
    }

    void applyParameter(uint32_t) override { valueChanged(value()); }

private:
    const SliderInfo fInfo;
    std::atomic<float> fValue;
};

// one parameter per JSFX slider; infos must outlive the plugin (a static array); states for
// plugins that keep more than their sliders (the Drum Machine's User pattern)
class NullMultiPlugin : public NullPluginBase
{
public:
    // more than kMaxParams sliders: only the first kMaxParams become parameters, which the
    // comparison test reports (the CLAP and the JSFX then differ in parameter count)
    NullMultiPlugin(const SliderInfo* infos, uint32_t count, uint32_t states = 0)
        : NullPluginBase(count < kMaxParams ? count : kMaxParams, states), fInfos(infos),
          fCount(count < kMaxParams ? count : kMaxParams)
    {
        DISTRHO_SAFE_ASSERT(count <= kMaxParams);
        for (uint32_t i = 0; i < fCount; ++i)
            fValues[i].store(infos[i].def, std::memory_order_relaxed);
    }

    static constexpr uint32_t kMaxParams = 64;
    static_assert(kMaxParams <= kMaxChangeBits, "one change bit per parameter");

    float value(uint32_t index) const noexcept
    {
        return index < fCount ? fValues[index].load(std::memory_order_relaxed) : 0.0f;
    }

protected:
    // called on the audio thread when a parameter has changed; derived classes apply the
    // defaults in their constructor (e.g. by calling their own update code)
    virtual void valueChanged(uint32_t index, float v) = 0;

    void initParameter(uint32_t index, Parameter& p) override
    {
        if (index < fCount)
            fillParameter(p, fInfos[index]);
    }

    float getParameterValue(uint32_t index) const override { return value(index); }

    // any thread
    void setParameterValue(uint32_t index, float v) override
    {
        if (index >= fCount)
            return;
        fValues[index].store(v, std::memory_order_relaxed);
        markChanged(index);
    }

    void applyParameter(uint32_t index) override { valueChanged(index, value(index)); }

private:
    const SliderInfo* fInfos;
    const uint32_t fCount;
    std::atomic<float> fValues[kMaxParams] = {};
};

END_NAMESPACE_DISTRHO
