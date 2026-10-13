/*
 * NULL JSFX - Filter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "FilterDSP.hpp"
#include "FilterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginFilter : public NullMultiPlugin
{
public:
    PluginFilter()
        : NullMultiPlugin(nullfilter::kSliders, nullfilter::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Filter"; }
    const char* getDescription() const override
    {
        return "Resonant state-variable filter: low pass, high pass, band pass or notch, with output gain.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '2', 'F', 'L'); }

    void valueChanged(uint32_t, float) override { update(); }

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
    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), int(std::lround(value(3))));
    }

    nullfilter::FilterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFilter)
};

Plugin* createPlugin()
{
    return new PluginFilter();
}

END_NAMESPACE_DISTRHO
