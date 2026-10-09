/*
 * NULL JSFX - Tilt (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "TiltDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginTilt : public NullOneSliderPlugin
{
public:
    PluginTilt()
        : NullOneSliderPlugin({ "Amount", "amount", -100.0f, 100.0f, 0.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Tilt"; }
    const char* getDescription() const override
    {
        return "Tilts the spectrum around 700 Hz: positive brightens, negative darkens, up to 6 dB each way.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'T', 'l', 't'); }

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
    nulltilt::TiltProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTilt)
};

Plugin* createPlugin()
{
    return new PluginTilt();
}

END_NAMESPACE_DISTRHO
