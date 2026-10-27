/*
 * NULL JSFX - Resonator (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ResonatorDSP.hpp"
#include "ResonatorSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginResonator : public NullMultiPlugin
{
public:
    PluginResonator()
        : NullMultiPlugin(nullres::kSliders, nullres::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Resonator"; }
    const char* getDescription() const override
    {
        return "Tuned string resonators: makes any input ring at a root note and chord, with decay, brightness and stereo spread.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 'e', 's'); }

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
    void update()
    {
        double s[nullres::kNumSliders];
        for (uint32_t i = 0; i < nullres::kNumSliders; ++i)
            s[i] = nullres::sliderValue(i, value(i));
        fDsp.setParams(s);
    }

    nullres::ResonatorProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginResonator)
};

Plugin* createPlugin()
{
    return new PluginResonator();
}

END_NAMESPACE_DISTRHO
