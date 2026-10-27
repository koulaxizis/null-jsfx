/*
 * NULL JSFX - Crossfeed (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "CrossfeedDSP.hpp"
#include "CrossfeedSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginCrossfeed : public NullMultiPlugin
{
public:
    PluginCrossfeed()
        : NullMultiPlugin(nullxfeed::kSliders, nullxfeed::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Crossfeed"; }
    const char* getDescription() const override
    {
        return "Headphone crossfeed (Bauer/bs2b) or two speakers in a room with head shadow, interaural time and early reflections.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'X', 'f', 'd'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
        }
    }

private:
    void update()
    {
        double v[nullxfeed::kNumSliders];
        for (uint32_t i = 0; i < nullxfeed::kNumSliders; ++i)
            v[i] = nullxfeed::sliderValue(i, value(i));
        fDsp.setParams(nullxfeed::params(v));
    }

    nullxfeed::CrossfeedProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginCrossfeed)
};

Plugin* createPlugin()
{
    return new PluginCrossfeed();
}

END_NAMESPACE_DISTRHO
