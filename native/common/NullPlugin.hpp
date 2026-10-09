/*
 * NULL JSFX - base class for the VST3/CLAP plugins of the One Slider series
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * One automatable parameter that mirrors slider1 of the JSFX, the shared maker/licence
 * metadata, and the Taps the UI reads for its display. A plugin implements getLabel,
 * getDescription, getUniqueId, valueChanged, activate/sampleRateChanged and run.
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
};

class NullOneSliderPlugin : public Plugin
{
public:
    explicit NullOneSliderPlugin(const SliderInfo& info)
        : Plugin(1, 0, 0), fInfo(info), fValue(info.def) {}

    nulljsfx::Taps taps;

    float value() const noexcept { return fValue; }

protected:
    // called whenever the parameter changes (also once from the constructor of the derived
    // class if it wants the default applied; call valueChanged(value()) there)
    virtual void valueChanged(float v) = 0;

    const char* getMaker() const override    { return "NULL JSFX"; }
    const char* getHomePage() const override { return "https://nulljsfx.tech"; }
    const char* getLicense() const override  { return "MIT"; }
    uint32_t getVersion() const override     { return d_version(2, 0, 0); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        if (index != 0)
            return;
        p.hints      = kParameterIsAutomatable;
        p.name       = fInfo.name;
        p.shortName  = fInfo.name;
        p.symbol     = fInfo.symbol;
        p.unit       = fInfo.unit;
        p.ranges.min = fInfo.min;
        p.ranges.max = fInfo.max;
        p.ranges.def = fInfo.def;
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

END_NAMESPACE_DISTRHO
