/*
 * NULL JSFX - Crunch (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "CrunchDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginCrunch : public NullOneSliderPlugin
{
public:
    PluginCrunch()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Crunch"; }
    const char* getDescription() const override
    {
        return "Hard, gritty clipping with a knee that hardens as Amount rises, a gentle 9 kHz roll-off and level compensation.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'C', 'r', 'n'); }

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
    nullcrunch::CrunchProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginCrunch)
};

Plugin* createPlugin()
{
    return new PluginCrunch();
}

END_NAMESPACE_DISTRHO
