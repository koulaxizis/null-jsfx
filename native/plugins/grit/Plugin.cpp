/*
 * NULL JSFX - Grit (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "GritDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginGrit : public NullOneSliderPlugin
{
public:
    PluginGrit()
        : NullOneSliderPlugin({ "Grit Intensity", "grit_intensity", 0.0f, 100.0f, 0.0f, "" })
    {
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Grit"; }
    const char* getDescription() const override
    {
        return "Lo-fi grit: sample-rate reduction (hold 1 to 8 samples) and bit reduction (16 down to 3 bits), crossfaded by Intensity.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'G', 'r', 't'); }

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
    nullgrit::GritProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGrit)
};

Plugin* createPlugin()
{
    return new PluginGrit();
}

END_NAMESPACE_DISTRHO
