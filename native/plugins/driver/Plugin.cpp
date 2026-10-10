/*
 * NULL JSFX - Driver (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DriverDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginDriver : public NullOneSliderPlugin
{
public:
    PluginDriver()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Driver"; }
    const char* getDescription() const override
    {
        return "Warm tube-style drive: an asymmetric tanh curve with DC removal and level compensation, faded in by Amount.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'r', 'v'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override
    {
        fDsp.reset();
    }

    void sampleRateChanged(double) override {}

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nulldriver::DriverProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDriver)
};

Plugin* createPlugin()
{
    return new PluginDriver();
}

END_NAMESPACE_DISTRHO
