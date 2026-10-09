/*
 * NULL JSFX - Stretch (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "StretchDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginStretch : public NullOneSliderPlugin
{
public:
    PluginStretch()
        : NullOneSliderPlugin({ "Amount", "amount", -100.0f, 100.0f, 0.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "Stretch"; }
    const char* getDescription() const override
    {
        return "Granular stretch: negative values drop the pitch with long smeared grains, positive values raise it with short ones.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 't', 'r'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullstretch::StretchProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStretch)
};

Plugin* createPlugin()
{
    return new PluginStretch();
}

END_NAMESPACE_DISTRHO
