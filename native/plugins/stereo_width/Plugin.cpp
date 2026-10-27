/*
 * NULL JSFX - Stereo Width (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "StereoWidthDSP.hpp"
#include "StereoWidthSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginStereoWidth : public NullMultiPlugin
{
public:
    PluginStereoWidth()
        : NullMultiPlugin(nullstereowidth::kSliders, nullstereowidth::kNumSliders)
    {
        fDsp.reset();
        update();
    }

protected:
    const char* getLabel() const override { return "StereoWidth"; }
    const char* getDescription() const override
    {
        return "Mid/side stereo width from mono to double, with mix and an optional mono-bass crossover.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '3', 's', 'w'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.reset();
        update();
    }

    void sampleRateChanged(double) override
    {
        fDsp.reset();
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        // display: vectorscope taps of the output (as in stereo_width_gfx.jsfx)
        for (uint32_t i = 0; i < frames; ++i)
            taps.push(outputs[0][i], outputs[1][i]);
    }

private:
    void update()
    {
        fDsp.setParams(nullstereowidth::paramsFor(value(0), value(1), value(2), getSampleRate()));
    }

    nullstereowidth::StereoWidthProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStereoWidth)
};

Plugin* createPlugin()
{
    return new PluginStereoWidth();
}

END_NAMESPACE_DISTRHO
