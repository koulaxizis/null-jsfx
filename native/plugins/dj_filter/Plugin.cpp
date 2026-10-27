/*
 * NULL JSFX - DJ Filter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DjFilterDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginDjFilter : public NullOneSliderPlugin
{
public:
    PluginDjFilter()
        : NullOneSliderPlugin({ "Filter", "filter", -100.0f, 100.0f, 0.0f, "", 1.0f })
    {
        reinit(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "DJFilter"; }
    const char* getDescription() const override
    {
        return "One-knob DJ filter: 24 dB/oct low-pass to the left, high-pass to the right, dry at the centre.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'j', 'F'); }

    void valueChanged(float v) override { fDsp.setFilter(v); }

    void onActivate() override { reinit(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reinit(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    // @init, then @slider with the current value
    void reinit(double sr)
    {
        fDsp.init(sr);
        fDsp.setFilter(value());
    }

    nulldjfilter::DjFilterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDjFilter)
};

Plugin* createPlugin()
{
    return new PluginDjFilter();
}

END_NAMESPACE_DISTRHO
