/*
 * NULL JSFX - Formant Filter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "FormantFilterDSP.hpp"
#include "FormantFilterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginFormantFilter : public NullMultiPlugin
{
public:
    PluginFormantFilter()
        : NullMultiPlugin(nullformant::kSliders, nullformant::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "FormantFilter"; }
    const char* getDescription() const override
    {
        return "Vowel filter: three band-passes at the formants of A, E, I, O or U, gliding between vowels.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '6', 'f', 'f'); }

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
    double v(uint32_t i) const { return nullformant::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(int(std::lround(v(0))), v(1), v(2)); }

    nullformant::FormantProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFormantFilter)
};

Plugin* createPlugin()
{
    return new PluginFormantFilter();
}

END_NAMESPACE_DISTRHO
