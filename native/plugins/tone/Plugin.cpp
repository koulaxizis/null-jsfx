/*
 * NULL JSFX - Tone (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ToneDSP.hpp"
#include "ToneSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTone : public NullMultiPlugin
{
public:
    PluginTone()
        : NullMultiPlugin(nulltone::kSliders, nulltone::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Tone"; }
    const char* getDescription() const override
    {
        return "Bass and treble: two broad 6 dB per octave shelves with movable corners, plus an output trim.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'T', 'o', 'n'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    double v(uint32_t i) const { return nulltone::sliderValue(i, value(i)); }

    void update()
    {
        nulltone::Params p;
        p.bass = v(0);
        p.treble = v(1);
        p.bassFreq = v(2);
        p.trebleFreq = v(3);
        p.output = v(4);
        fDsp.setParams(p);
    }

    nulltone::ToneProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTone)
};

Plugin* createPlugin()
{
    return new PluginTone();
}

END_NAMESPACE_DISTRHO
