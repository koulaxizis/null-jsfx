/*
 * NULL JSFX - Binaural Panner (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "BinauralPannerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginBinauralPanner : public NullMultiPlugin
{
public:
    PluginBinauralPanner()
        : NullMultiPlugin(nullbp::kSliders, nullbp::kNumSliders)
    {
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "BinauralPanner"; }
    const char* getDescription() const override
    {
        return "Places a sound around the head for headphones: spherical-head model with delay, head shadow, pinna cues, distance and room.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'i', 'n'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void update()
    {
        float v[nullbp::kNumSliders];
        for (uint32_t i = 0; i < nullbp::kNumSliders; ++i)
            v[i] = value(i);
        nullbp::apply(fDsp, v);
    }

    nullbp::BinauralPanner fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBinauralPanner)
};

Plugin* createPlugin()
{
    return new PluginBinauralPanner();
}

END_NAMESPACE_DISTRHO
