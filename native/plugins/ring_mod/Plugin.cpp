/*
 * NULL JSFX - Ring Mod (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "RingModDSP.hpp"
#include "RingModSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginRingMod : public NullMultiPlugin
{
public:
    PluginRingMod()
        : NullMultiPlugin(nullringmod::kSliders, nullringmod::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "RingMod"; }
    const char* getDescription() const override
    {
        return "Ring modulator: a sine carrier swept by an LFO up to an octave; rate, carrier, depth and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'r', 'm'); }

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
        using nullringmod::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 20.0, 0.1), onGrid(value(1), 100.0, 2000.0, 10.0), value(2), value(3));
    }

    nullringmod::RingModProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRingMod)
};

Plugin* createPlugin()
{
    return new PluginRingMod();
}

END_NAMESPACE_DISTRHO
