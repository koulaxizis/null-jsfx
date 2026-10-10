/*
 * NULL JSFX - Doubler (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DoublerDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginDoubler : public NullOneSliderPlugin
{
public:
    PluginDoubler()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 40.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Doubler"; }
    const char* getDescription() const override
    {
        return "Automatic double tracking: two slowly drifting 12-30 ms copies placed left and right around the original.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'b', 'l'); }

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
    nulldoubler::DoublerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDoubler)
};

Plugin* createPlugin()
{
    return new PluginDoubler();
}

END_NAMESPACE_DISTRHO
