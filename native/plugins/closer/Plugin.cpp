/*
 * NULL JSFX - Closer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "CloserDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginCloser : public NullOneSliderPlugin
{
public:
    PluginCloser()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 40.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "Closer"; }
    const char* getDescription() const override
    {
        return "One knob that brings a voice forward: light compression, dynamic presence and proximity.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'C', 'l', 's'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void onActivate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullcloser::CloserProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginCloser)
};

Plugin* createPlugin()
{
    return new PluginCloser();
}

END_NAMESPACE_DISTRHO
