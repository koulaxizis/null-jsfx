/*
 * NULL JSFX - Ladder Filter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "LadderFilterDSP.hpp"
#include "LadderFilterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginLadderFilter : public NullMultiPlugin
{
public:
    PluginLadderFilter()
        : NullMultiPlugin(nullladder::kSliders, nullladder::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "LadderFilter"; }
    const char* getDescription() const override
    {
        return "Moog-style 24 dB/oct zero-delay-feedback ladder, lowpass or highpass, with resonance and drive.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '6', 'l', 'f'); }

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
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    double v(uint32_t i) const { return nullladder::sliderValue(i, value(i)); }

    void update()
    {
        fDsp.setParams(v(0), v(1), v(2), int(std::lround(v(3))), v(4), int(std::lround(v(5))));
        setLatency(uint32_t(fDsp.latency())); // pdc_delay
    }

    nullladder::LadderProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginLadderFilter)
};

Plugin* createPlugin()
{
    return new PluginLadderFilter();
}

END_NAMESPACE_DISTRHO
