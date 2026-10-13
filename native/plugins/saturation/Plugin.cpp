/*
 * NULL JSFX - Saturation (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SaturationDSP.hpp"
#include "SaturationSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSaturation : public NullMultiPlugin
{
public:
    PluginSaturation()
        : NullMultiPlugin(nullsaturation::kSliders, nullsaturation::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Saturation"; }
    const char* getDescription() const override
    {
        return "Seven flavours of saturation (tape, tube, overdrive, fuzz, bitcrush, waveshaper, sine wavefolder) with tone and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '5', 'S', 'a'); }

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
    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), int(std::lround(value(3))));
    }

    nullsaturation::SaturationProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSaturation)
};

Plugin* createPlugin()
{
    return new PluginSaturation();
}

END_NAMESPACE_DISTRHO
