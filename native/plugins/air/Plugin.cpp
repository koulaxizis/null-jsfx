/*
 * NULL JSFX - Air (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "AirDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginAir : public NullOneSliderPlugin
{
public:
    PluginAir()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 0.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Air"; }
    const char* getDescription() const override
    {
        return "A broad high shelf for the very top: up to 9 dB, its corner moving from 16 kHz down to 10 kHz.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'A', 'i', 'r'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullair::AirProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAir)
};

Plugin* createPlugin()
{
    return new PluginAir();
}

END_NAMESPACE_DISTRHO
