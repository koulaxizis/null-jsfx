/*
 * NULL JSFX - Stereo Scope (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "StereoScopeSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginStereoScope : public StereoScopeBase
{
public:
    PluginStereoScope()
    {
        fDsp.setView(&view);
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "StereoScope"; }
    const char* getDescription() const override
    {
        return "Goniometer, correlation, balance and width meter; the audio passes through untouched.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'c', 'p'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void update() { fDsp.setParams(value(3), int(std::lround(value(4)))); }

    nullscope::StereoScope fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStereoScope)
};

Plugin* createPlugin()
{
    return new PluginStereoScope();
}

END_NAMESPACE_DISTRHO
