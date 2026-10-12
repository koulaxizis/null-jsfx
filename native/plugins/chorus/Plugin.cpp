/*
 * NULL JSFX - Chorus (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ChorusDSP.hpp"
#include "ChorusSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginChorus : public NullMultiPlugin
{
public:
    PluginChorus()
        : NullMultiPlugin(nullchorus::kSliders, nullchorus::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Chorus"; }
    const char* getDescription() const override
    {
        return "Stereo chorus: a sine LFO sweeps a 5 ms + depth delay line; rate, depth and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'c', 'h'); }

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
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nullchorus::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 20.0, 0.1), onGrid(value(1), 0.0, 10.0, 0.1), value(2));
    }

    nullchorus::ChorusProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChorus)
};

Plugin* createPlugin()
{
    return new PluginChorus();
}

END_NAMESPACE_DISTRHO
