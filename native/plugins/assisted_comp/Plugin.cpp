/*
 * NULL JSFX - Assisted Comp (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "AssistedCompDSP.hpp"
#include "AssistedCompSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginAssistedComp : public NullMultiPlugin
{
public:
    PluginAssistedComp()
        : NullMultiPlugin(nullassistedcomp::kSliders, nullassistedcomp::kNumSliders)
    {
        for (uint32_t i = 0; i < nullassistedcomp::kNumSliders; ++i)
            fDsp.s[i] = nullassistedcomp::sliderValue(i, value(i));
        fDsp.setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "AssistedComp"; }
    const char* getDescription() const override
    {
        return "A compressor that sets itself: Learn listens to the track, then threshold, ratio, attack, release and makeup follow the chosen target.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'A', 'C', 'o'); }

    void valueChanged(uint32_t index, float v) override
    {
        fDsp.s[index] = nullassistedcomp::sliderValue(index, v);
        fDsp.update();
    }

    void onActivate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        bool learned = fDsp.block();
        learned = fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames) || learned;
        if (learned)
        {
            // Learn wrote its sliders (and set Learn back to Off), as the JSFX does; the
            // interface sees the count go up and tells the host
            setOwnValue(0, float(fDsp.s[0]));
            for (uint32_t i = 8; i < nullassistedcomp::kNumSliders; ++i)
                setOwnValue(i, float(fDsp.s[i]));
            taps.aux[1].store(taps.aux[1].load(std::memory_order_relaxed) + 1.0f, std::memory_order_release);
        }
        // for the interface: gain reduction, listening or not, how far, and seconds heard
        taps.gr.store(float(fDsp.reduction()), std::memory_order_relaxed);
        taps.aux[2].store(fDsp.learning() ? 1.0f : 0.0f, std::memory_order_relaxed);
        taps.extra.store(float(fDsp.progress()), std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.heard()), std::memory_order_relaxed);
    }

private:
    nullassistedcomp::AssistedCompProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAssistedComp)
};

Plugin* createPlugin()
{
    return new PluginAssistedComp();
}

END_NAMESPACE_DISTRHO
