/*
 * NULL JSFX - Disperser (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DisperserDSP.hpp"
#include "DisperserSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDisperser : public NullMultiPlugin
{
public:
    PluginDisperser()
        : NullMultiPlugin(nulldisperser::kSliders, nulldisperser::kNumSliders)
    {
        fDsp.init(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Disperser"; }
    const char* getDescription() const override
    {
        return "Phase dispersion: up to 64 all-pass filters around a frequency smear transients into laser chirps or tighten kicks.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 's', 'p'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.init(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.init(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    double v(uint32_t i) const { return nulldisperser::sliderValue(i, value(i)); }

    void update()
    {
        nulldisperser::Params p;
        p.freq = v(0);
        p.amount = v(1);
        p.pinch = v(2);
        p.mix = v(3);
        p.mode = int(v(4));
        fDsp.setParams(p);
    }

    nulldisperser::DisperserProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDisperser)
};

Plugin* createPlugin()
{
    return new PluginDisperser();
}

END_NAMESPACE_DISTRHO
