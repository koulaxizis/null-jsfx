/*
 * NULL JSFX - Convolution: the plugin base the interface reads (direct access)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * NullMultiPlugin has no state; Convolution needs one (the IR file), so its plugin derives from
 * this base instead: one parameter per slider (as NullMultiPlugin) plus the "irfile" state, and
 * the IR envelope / length the display shows.
 */

#pragma once

#include "DistrhoPlugin.hpp"
#include "NullPlugin.hpp"
#include "ConvolutionDSP.hpp"

#include <atomic>

START_NAMESPACE_DISTRHO

class ConvolutionPluginBase : public Plugin
{
public:
    ConvolutionPluginBase(const SliderInfo* infos, uint32_t count)
        : Plugin(count, 0, 1), fInfos(infos), fCount(count)
    {
        for (uint32_t i = 0; i < count && i < kMaxParams; ++i)
            fValues[i] = infos[i].def;
    }

    static constexpr uint32_t kMaxParams = 16;

    // what the display reads: the IR peak per 15 ms over 3 s, its length in seconds, and
    // whether it exists (an IR File that failed to load gives none)
    float envelope[nullconv::kEnvBins] = {};
    std::atomic<float> irSeconds { 0.0f };

    float value(uint32_t index) const noexcept { return index < fCount ? fValues[index] : 0.0f; }

protected:
    const char* getMaker() const override    { return "NULL JSFX"; }
    const char* getHomePage() const override { return "https://nulljsfx.tech"; }
    const char* getLicense() const override  { return "MIT"; }
    uint32_t getVersion() const override     { return d_version(2, 0, 0); }

    virtual void valueChanged(uint32_t index, float v) = 0;

    void initParameter(uint32_t index, Parameter& p) override
    {
        if (index >= fCount)
            return;
        const SliderInfo& in = fInfos[index];
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

    float getParameterValue(uint32_t index) const override { return value(index); }

    void setParameterValue(uint32_t index, float v) override
    {
        if (index >= fCount)
            return;
        fValues[index] = v;
        valueChanged(index, v);
    }

    void initState(uint32_t index, State& state) override
    {
        if (index != 0)
            return;
        state.hints = kStateIsFilenamePath;
        state.key = "irfile";
        state.defaultValue = "";
        state.label = "IR File";
        state.description = "WAV impulse response used when Source is IR File";
    }

private:
    const SliderInfo* fInfos;
    const uint32_t fCount;
    float fValues[kMaxParams] = {};
};

END_NAMESPACE_DISTRHO
