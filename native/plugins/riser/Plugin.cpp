/*
 * NULL JSFX - Riser (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "RiserDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginRiser : public NullOneSliderPlugin
{
public:
    PluginRiser()
        : NullOneSliderPlugin({ "Intensity", "intensity", 0.0f, 100.0f, 0.0f, "%" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "Riser"; }
    const char* getDescription() const override
    {
        return "Build-up in one control: a rising resonant high-pass, a swelling noise band, a wider image and a diffuse wash.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 's', 'r'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullriser::RiserProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRiser)
};

Plugin* createPlugin()
{
    return new PluginRiser();
}

END_NAMESPACE_DISTRHO
