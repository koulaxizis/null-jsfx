/*
 * NULL JSFX - Frequency Shifter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "FrequencyShifterDSP.hpp"
#include "FrequencyShifterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginFrequencyShifter : public NullMultiPlugin
{
public:
    PluginFrequencyShifter()
        : NullMultiPlugin(nullfreqshift::kSliders, nullfreqshift::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "FrequencyShifter"; }
    const char* getDescription() const override
    {
        return "Bode frequency shifter: moves every frequency by the same number of Hz, with stereo offset and barber-pole feedback.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '6', 'f', 's'); }

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
    double v(uint32_t i) const { return nullfreqshift::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(v(0), v(1), v(2), v(3)); }

    nullfreqshift::FreqShiftProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFrequencyShifter)
};

Plugin* createPlugin()
{
    return new PluginFrequencyShifter();
}

END_NAMESPACE_DISTRHO
