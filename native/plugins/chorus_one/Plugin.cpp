/*
 * NULL JSFX - Chorus One (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ChorusOneDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginChorusOne : public NullOneSliderPlugin
{
public:
    PluginChorusOne()
        : NullOneSliderPlugin({ "Chorus Amount", "chorus_amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "ChorusOne"; }
    const char* getDescription() const override
    {
        return "Stereo chorus: a triangle LFO sweeps two 5-12 ms delay lines in opposite directions, up to 85% wet.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'C', 'h', 'o'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullchorusone::ChorusOneProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChorusOne)
};

Plugin* createPlugin()
{
    return new PluginChorusOne();
}

END_NAMESPACE_DISTRHO
