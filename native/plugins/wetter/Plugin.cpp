/*
 * NULL JSFX - Wetter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "WetterDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginWetter : public NullOneSliderPlugin
{
public:
    PluginWetter()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "Wetter"; }
    const char* getDescription() const override
    {
        return "Instant room: a compact Freeverb-style reverb; Amount raises the wet level and the room size together.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'W', 'e', 't'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullwetter::WetterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWetter)
};

Plugin* createPlugin()
{
    return new PluginWetter();
}

END_NAMESPACE_DISTRHO
