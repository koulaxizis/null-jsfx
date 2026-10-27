/*
 * NULL JSFX - OTT (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "OttDSP.hpp"
#include "OttSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginOtt : public NullMultiPlugin
{
public:
    PluginOtt()
        : NullMultiPlugin(nullott::kSliders, nullott::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "OTT"; }
    const char* getDescription() const override
    {
        return "Over-the-top three-band upward and downward compression, with depth, time and band gains.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '9', 'O', 't'); }

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
        // the band gains for the display (gl, gm, gh), in the first slots of the scope buffer
        for (int b = 0; b < 3; ++b)
            taps.scopeL[b] = float(fDsp.bandGain(b));
    }

private:
    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4), value(5), value(6), value(7));
    }

    nullott::OttProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginOtt)
};

Plugin* createPlugin()
{
    return new PluginOtt();
}

END_NAMESPACE_DISTRHO
