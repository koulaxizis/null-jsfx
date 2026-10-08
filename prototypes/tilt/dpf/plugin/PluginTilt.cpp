/*
 * NULL Tilt Native - DPF plugin (DSP side)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "DistrhoPlugin.hpp"
#include "TiltDSP.hpp"

START_NAMESPACE_DISTRHO

enum Parameters {
    kParamAmount = 0,
    kParamCount
};

class PluginTilt : public Plugin
{
public:
    PluginTilt()
        : Plugin(kParamCount, 0, 0)
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(0.0);
        fDsp.reset();
    }

protected:
    const char* getLabel() const override       { return "NullTiltNative"; }
    const char* getDescription() const override
    {
        return "Tilts the spectrum around 700 Hz: positive brightens, negative darkens, up to 6 dB each way.";
    }
    const char* getMaker() const override       { return "NULL JSFX"; }
    const char* getHomePage() const override    { return "https://nulljsfx.tech"; }
    const char* getLicense() const override     { return "MIT"; }
    uint32_t getVersion() const override        { return d_version(2, 0, 0); }
    int64_t getUniqueId() const override        { return d_cconst('N', 'T', 'l', 't'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index != kParamAmount)
            return;

        parameter.hints      = kParameterIsAutomatable;
        parameter.name       = "Amount";
        parameter.shortName  = "Amount";
        parameter.symbol     = "amount";
        parameter.unit       = "";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -100.0f;
        parameter.ranges.max = 100.0f;
    }

    float getParameterValue(uint32_t index) const override
    {
        return index == kParamAmount ? static_cast<float>(fDsp.getAmount()) : 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index == kParamAmount)
            fDsp.setAmount(value);
    }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nulltilt::TiltProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTilt)
};

Plugin* createPlugin()
{
    return new PluginTilt();
}

END_NAMESPACE_DISTRHO
