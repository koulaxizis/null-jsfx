/*
 * NULL JSFX - Sweeper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SweeperDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginSweeper : public NullOneSliderPlugin
{
public:
    PluginSweeper()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Sweeper"; }
    const char* getDescription() const override
    {
        return "DJ-style filter sweep: 50 is open, below closes a resonant low-pass, above raises a resonant high-pass.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'w', 'p'); }

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
    nullsweeper::SweeperProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSweeper)
};

Plugin* createPlugin()
{
    return new PluginSweeper();
}

END_NAMESPACE_DISTRHO
