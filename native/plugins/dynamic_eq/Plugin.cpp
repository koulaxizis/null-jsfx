/*
 * NULL JSFX - Dynamic EQ (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DynamicEqDSP.hpp"
#include "DynamicEqSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDynamicEq : public NullMultiPlugin
{
public:
    PluginDynamicEq()
        : NullMultiPlugin(nulldeq::kSliders, nulldeq::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "DynamicEQ"; }
    const char* getDescription() const override
    {
        return "Four bell bands that only act when their band passes its threshold: tame resonances or lift a band when it plays.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '2', 'D', 'E'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        // the four band envelopes for the display: inLevel, outLevel, gr, extra = band 1..4
        taps.inLevel.store(float(fDsp.envelope(0)), std::memory_order_relaxed);
        taps.outLevel.store(float(fDsp.envelope(1)), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.envelope(2)), std::memory_order_relaxed);
        taps.extra.store(float(fDsp.envelope(3)), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nulldeq::params([this](uint32_t i) { return double(value(i)); }));
    }

    nulldeq::DynamicEqProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDynamicEq)
};

Plugin* createPlugin()
{
    return new PluginDynamicEq();
}

END_NAMESPACE_DISTRHO
