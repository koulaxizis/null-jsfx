/*
 * NULL JSFX - Sub Bloom (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SubBloomDSP.hpp"
#include "SubBloomSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSubBloom : public NullMultiPlugin
{
public:
    PluginSubBloom()
        : NullMultiPlugin(nullsubbloom::kSliders, nullsubbloom::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "SubBloom"; }
    const char* getDescription() const override
    {
        return "Subharmonic synthesizer: tracks the bass note and adds a sub one or two octaves below it.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '6', 's', 'b'); }

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
    double v(uint32_t i) const { return nullsubbloom::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(v(0), v(1), int(std::lround(v(2))), v(3), v(4)); }

    nullsubbloom::SubBloomProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSubBloom)
};

Plugin* createPlugin()
{
    return new PluginSubBloom();
}

END_NAMESPACE_DISTRHO
