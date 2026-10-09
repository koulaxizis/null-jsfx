/*
 * NULL JSFX - Formant Shift (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "FormantShiftDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginFormantShift : public NullOneSliderPlugin
{
public:
    PluginFormantShift()
        : NullOneSliderPlugin({ "Formant Position", "formant_position", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Formant Shift"; }
    const char* getDescription() const override
    {
        return "Blends in three resonant vocal formant peaks, shifted down (deep) or up (high) by up to an octave.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'F', 'm', 't'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    nullformant::FormantShiftProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFormantShift)
};

Plugin* createPlugin()
{
    return new PluginFormantShift();
}

END_NAMESPACE_DISTRHO
