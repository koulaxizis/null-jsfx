/*
 * NULL JSFX - Vibrato (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "VibratoDSP.hpp"
#include "VibratoSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginVibrato : public NullMultiPlugin
{
public:
    PluginVibrato()
        : NullMultiPlugin(nullvibrato::kSliders, nullvibrato::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Vibrato"; }
    const char* getDescription() const override
    {
        return "Vibrato: a sine LFO sweeps a short delay line to bend the pitch; rate, depth and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'v', 'b'); }

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
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nullvibrato::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 15.0, 0.1), onGrid(value(1), 0.0, 20.0, 0.1), value(2));
    }

    nullvibrato::VibratoProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginVibrato)
};

Plugin* createPlugin()
{
    return new PluginVibrato();
}

END_NAMESPACE_DISTRHO
