/*
 * NULL JSFX - Tape (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "TapeDSP.hpp"
#include "TapeSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTape : public NullMultiPlugin
{
public:
    PluginTape()
        : NullMultiPlugin(nulltape::kSliders, nulltape::kNumSliders)
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Tape"; }
    const char* getDescription() const override
    {
        return "Tape machine: saturation with emphasis, head bump and high-frequency loss per tape speed, wow, flutter and hiss.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'T', 'a', 'p'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { reset(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reset(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    // @init then @slider
    void reset(double sr)
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void update()
    {
        double s[nulltape::kNumSliders];
        for (uint32_t i = 0; i < nulltape::kNumSliders; ++i)
            s[i] = nulltape::sliderValue(i, value(i));
        fDsp.setParams(s);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay
    }

    nulltape::TapeProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTape)
};

Plugin* createPlugin()
{
    return new PluginTape();
}

END_NAMESPACE_DISTRHO
