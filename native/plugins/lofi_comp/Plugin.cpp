/*
 * NULL JSFX - Lo-Fi Comp (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "LofiCompDSP.hpp"
#include "LofiCompSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginLofiComp : public NullMultiPlugin
{
public:
    PluginLofiComp()
        : NullMultiPlugin(nulllofi::kSliders, nulllofi::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "LofiComp"; }
    const char* getDescription() const override
    {
        return "The lo-fi beat sound in one effect: a pumping compressor, a slow pitch drift and an old sampler.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'L', 'o', 'C'); }

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
        taps.inLevel.store(float(fDsp.env()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.grDb()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        double v[nulllofi::kNumSliders];
        for (uint32_t i = 0; i < nulllofi::kNumSliders; ++i)
            v[i] = jsfxValue(nulllofi::kSliders[i], value(i));
        fDsp.setParams(nulllofi::paramsFor(v, getSampleRate()));
    }

    nulllofi::LofiCompProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginLofiComp)
};

Plugin* createPlugin()
{
    return new PluginLofiComp();
}

END_NAMESPACE_DISTRHO
