/*
 * NULL JSFX - Panner (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PannerDSP.hpp"
#include "PannerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPanner : public NullMultiPlugin
{
public:
    PluginPanner()
        : NullMultiPlugin(nullpanner::kSliders, nullpanner::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Panner"; }
    const char* getDescription() const override
    {
        return "Places a source in the stereo field: balance, a natural head model (ITD, ILD, head shadow) or Haas delay.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'a', 'n'); }

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
        // display: vectorscope taps of the output (as in panner_gfx.jsfx)
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames,
                     [this](double l, double r) { taps.push(float(l), float(r)); });
    }

private:
    double v(uint32_t i) const { return nullpanner::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(v(0), int(std::lround(v(1))), v(2), v(3), v(4)); }

    nullpanner::PannerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPanner)
};

Plugin* createPlugin()
{
    return new PluginPanner();
}

END_NAMESPACE_DISTRHO
