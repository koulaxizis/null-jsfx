/*
 * NULL JSFX - Reverb (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ReverbDSP.hpp"
#include "ReverbSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginReverb : public NullMultiPlugin
{
public:
    PluginReverb()
        : NullMultiPlugin(nullreverb::kSliders, nullreverb::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Reverb"; }
    const char* getDescription() const override
    {
        return "Stereo room, plate and hall reverb: size, decay, damping, mix, pre-delay, low and high cut, width, modulation and freeze.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '5', 'R', 'v'); }

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
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4) > 0.0f, value(5), value(6), value(7), value(8),
                       value(9), value(10) > 0.0f, int(std::lround(value(11))));
    }

    nullreverb::ReverbProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginReverb)
};

Plugin* createPlugin()
{
    return new PluginReverb();
}

END_NAMESPACE_DISTRHO
