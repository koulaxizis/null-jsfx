/*
 * NULL JSFX - Comb Filter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "CombFilterDSP.hpp"
#include "CombFilterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginCombFilter : public NullMultiPlugin
{
public:
    PluginCombFilter()
        : NullMultiPlugin(nullcomb::kSliders, nullcomb::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "CombFilter"; }
    const char* getDescription() const override
    {
        return "Comb filter: peaks at a frequency and its harmonics, feed-forward or ringing feed-backward, level-compensated.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '6', 'c', 'f'); }

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
    double v(uint32_t i) const { return nullcomb::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(v(0), v(1), int(std::lround(v(2))), v(3)); }

    nullcomb::CombProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginCombFilter)
};

Plugin* createPlugin()
{
    return new PluginCombFilter();
}

END_NAMESPACE_DISTRHO
