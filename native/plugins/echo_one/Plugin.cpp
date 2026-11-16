/*
 * NULL JSFX - Echo One (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "EchoOneDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginEchoOne : public NullOneSliderPlugin
{
public:
    PluginEchoOne()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 40.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "EchoOne"; }
    const char* getDescription() const override
    {
        return "One-knob tempo delay: from a soft 1/16 slapback to a long 1/4 ping-pong with darker repeats.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'E', 'c', 'O'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void onActivate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo);
    }

private:
    nullechoone::EchoOneProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEchoOne)
};

Plugin* createPlugin()
{
    return new PluginEchoOne();
}

END_NAMESPACE_DISTRHO
