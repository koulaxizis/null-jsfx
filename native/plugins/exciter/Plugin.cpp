/*
 * NULL JSFX - Exciter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ExciterDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginExciter : public NullOneSliderPlugin
{
public:
    PluginExciter()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 30.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Exciter"; }
    const char* getDescription() const override
    {
        return "Harmonic exciter: saturates the highs above about 3 kHz and blends the new upper harmonics back in.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'E', 'x', 'c'); }

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
    nullexciter::ExciterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginExciter)
};

Plugin* createPlugin()
{
    return new PluginExciter();
}

END_NAMESPACE_DISTRHO
