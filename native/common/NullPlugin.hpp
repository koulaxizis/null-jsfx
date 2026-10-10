/*
 * NULL JSFX - base classes for the VST3/CLAP plugins
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * NullOneSliderPlugin: one automatable parameter that mirrors slider1 of the JSFX.
 * NullMultiPlugin: one parameter per JSFX slider, in the order the JSFX declares them.
 * Both carry the shared maker/licence metadata and the Taps the UI reads for its display.
 * A plugin implements getLabel, getDescription, getUniqueId, valueChanged,
 * activate/sampleRateChanged and run.
 */

#pragma once

#include "DistrhoPlugin.hpp"
#include "NullTaps.hpp"

START_NAMESPACE_DISTRHO

struct SliderInfo {
    const char* name;   // e.g. "Amount"
    const char* symbol; // e.g. "amount"
    float min, max, def;
    const char* unit;   // e.g. "%" or ""
    float step = 0;     // the JSFX slider step; 1 or more makes an integer parameter
    const char* const* options = nullptr; // enum labels for min, min+1, ... max (JSFX {A,B,C})
};

// the Taps (meters, scopes) and the metadata every NULL JSFX plugin shares
class NullPluginBase : public Plugin
{
public:
    explicit NullPluginBase(uint32_t parameters)
        : Plugin(parameters, 0, 0) {}

    nulljsfx::Taps taps;

protected:
    const char* getMaker() const override    { return "NULL JSFX"; }
    const char* getHomePage() const override { return "https://nulljsfx.tech"; }
    const char* getLicense() const override  { return "MIT"; }
    uint32_t getVersion() const override     { return d_version(2, 0, 0); }

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
};

class NullOneSliderPlugin : public NullPluginBase
{
public:
    explicit NullOneSliderPlugin(const SliderInfo& info)
        : NullPluginBase(1), fInfo(info), fValue(info.def) {}

    float value() const noexcept { return fValue; }

protected:
    // called whenever the parameter changes (also once from the constructor of the derived
    // class if it wants the default applied; call valueChanged(value()) there)
    virtual void valueChanged(float v) = 0;

    void initParameter(uint32_t index, Parameter& p) override
    {
        if (index == 0)
            fillParameter(p, fInfo);
    }

    float getParameterValue(uint32_t index) const override { return index == 0 ? fValue : 0.0f; }

    void setParameterValue(uint32_t index, float v) override
    {
        if (index != 0)
            return;
        fValue = v;
        valueChanged(v);
    }

private:
    const SliderInfo fInfo;
    float fValue;
};

// one parameter per JSFX slider; infos must outlive the plugin (a static array)
class NullMultiPlugin : public NullPluginBase
{
public:
    NullMultiPlugin(const SliderInfo* infos, uint32_t count)
        : NullPluginBase(count), fInfos(infos), fCount(count)
    {
        for (uint32_t i = 0; i < count && i < kMaxParams; ++i)
            fValues[i] = infos[i].def;
    }

    static constexpr uint32_t kMaxParams = 64;

    float value(uint32_t index) const noexcept { return index < fCount ? fValues[index] : 0.0f; }

protected:
    // called whenever a parameter changes; derived classes apply the defaults in their
    // constructor (e.g. by calling their own update code)
    virtual void valueChanged(uint32_t index, float v) = 0;

    void initParameter(uint32_t index, Parameter& p) override
    {
        if (index < fCount)
            fillParameter(p, fInfos[index]);
    }

    float getParameterValue(uint32_t index) const override { return value(index); }

    void setParameterValue(uint32_t index, float v) override
    {
        if (index >= fCount)
            return;
        fValues[index] = v;
        valueChanged(index, v);
    }

private:
    const SliderInfo* fInfos;
    const uint32_t fCount;
    float fValues[kMaxParams] = {};
};

END_NAMESPACE_DISTRHO
