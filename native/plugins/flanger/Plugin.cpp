/*
 * NULL JSFX - Flanger (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "FlangerDSP.hpp"
#include "FlangerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginFlanger : public NullMultiPlugin
{
public:
    PluginFlanger()
        : NullMultiPlugin(nullflanger::kSliders, nullflanger::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Flanger"; }
    const char* getDescription() const override
    {
        return "Stereo flanger: a sine LFO sweeps a short delay line with feedback; rate, depth, feedback and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'f', 'l'); }

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
        using nullflanger::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 5.0, 0.1), onGrid(value(1), 1.0, 20.0, 0.1), value(2), value(3));
    }

    nullflanger::FlangerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFlanger)
};

Plugin* createPlugin()
{
    return new PluginFlanger();
}

END_NAMESPACE_DISTRHO
