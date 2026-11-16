/*
 * NULL JSFX - Bloom Delay (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "BloomDelayDSP.hpp"
#include "BloomDelaySliders.hpp"

START_NAMESPACE_DISTRHO

class PluginBloomDelay : public NullMultiPlugin
{
public:
    PluginBloomDelay()
        : NullMultiPlugin(nullbloom::kSliders, nullbloom::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "BloomDelay"; }
    const char* getDescription() const override
    {
        return "A delay whose repeats bloom into a reverb, with tempo sync, modulation and safe high feedback.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'l', 'D'); }

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
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo);
    }

private:
    void update()
    {
        double v[nullbloom::kNumSliders];
        for (uint32_t i = 0; i < nullbloom::kNumSliders; ++i)
            v[i] = jsfxValue(nullbloom::kSliders[i], value(i));
        fDsp.setParams(v);
    }

    nullbloom::BloomDelayProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBloomDelay)
};

Plugin* createPlugin()
{
    return new PluginBloomDelay();
}

END_NAMESPACE_DISTRHO
