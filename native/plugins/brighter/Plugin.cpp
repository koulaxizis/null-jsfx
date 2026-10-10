/*
 * NULL JSFX - Brighter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "BrighterDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginBrighter : public NullOneSliderPlugin
{
public:
    PluginBrighter()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Brighter"; }
    const char* getDescription() const override
    {
        return "Adds air and presence: a high shelf above about 6 kHz, up to +9 dB.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'r', 't'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullbrighter::BrighterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBrighter)
};

Plugin* createPlugin()
{
    return new PluginBrighter();
}

END_NAMESPACE_DISTRHO
