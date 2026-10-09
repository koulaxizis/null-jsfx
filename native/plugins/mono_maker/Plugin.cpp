/*
 * NULL JSFX - Mono Maker (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "MonoMakerDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginMonoMaker : public NullOneSliderPlugin
{
public:
    PluginMonoMaker()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "MonoMaker"; }
    const char* getDescription() const override
    {
        return "Removes the side (L-R) content below a crossover of up to 400 Hz so the low end becomes mono.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'o', 'n'); }

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
    nullmonomaker::MonoMakerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMonoMaker)
};

Plugin* createPlugin()
{
    return new PluginMonoMaker();
}

END_NAMESPACE_DISTRHO
