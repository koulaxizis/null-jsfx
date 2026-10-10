/*
 * NULL JSFX - Phatter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PhatterDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginPhatter : public NullOneSliderPlugin
{
public:
    PluginPhatter()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Phatter"; }
    const char* getDescription() const override
    {
        return "Adds weight to the low end: a low shelf below about 200 Hz, up to +12 dB.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'h', 't'); }

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
    nullphatter::PhatterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPhatter)
};

Plugin* createPlugin()
{
    return new PluginPhatter();
}

END_NAMESPACE_DISTRHO
