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
        return "Stereo room-to-hall reverb: room size, decay, high-frequency damping and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '5', 'R', 'v'); }

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
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4) > 0.0f);
    }

    nullreverb::ReverbProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginReverb)
};

Plugin* createPlugin()
{
    return new PluginReverb();
}

END_NAMESPACE_DISTRHO
