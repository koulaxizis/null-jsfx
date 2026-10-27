/*
 * NULL JSFX - De-Clip (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DeClipDSP.hpp"
#include "DeClipSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDeClip : public NullMultiPlugin
{
public:
    PluginDeClip()
        : NullMultiPlugin(nulldeclip::kSliders, nulldeclip::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        setLatency(uint32_t(fDsp.latency())); // the JSFX's pdc_delay
    }

protected:
    const char* getLabel() const override { return "DeClip"; }
    const char* getDescription() const override
    {
        return "Rebuilds clipped peaks: runs of samples stuck at the clip level are redrawn as the curve through the good samples around them.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'c', 'l'); }

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
            taps.push(float(fDsp.lastInMid()), float((l + r) * 0.5));
        }
        taps.extra.store(float(fDsp.runs()), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nulldeclip::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(nulldeclip::params(v(0), v(1), v(2), v(3))); }

    nulldeclip::DeClipProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeClip)
};

Plugin* createPlugin()
{
    return new PluginDeClip();
}

END_NAMESPACE_DISTRHO
