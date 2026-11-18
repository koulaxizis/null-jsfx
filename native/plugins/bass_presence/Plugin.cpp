/*
 * NULL JSFX - Bass Presence (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "BassPresenceDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginBassPresence : public NullOneSliderPlugin
{
public:
    PluginBassPresence()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 40.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "BassPresence"; }
    const char* getDescription() const override
    {
        return "One-knob bass presence: harmonics of the lows added so bass is heard on small speakers.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'P', 's'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void onActivate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullbasspresence::BassPresenceProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBassPresence)
};

Plugin* createPlugin()
{
    return new PluginBassPresence();
}

END_NAMESPACE_DISTRHO
