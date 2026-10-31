/*
 * NULL JSFX - Assisted Reverb (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "AssistedReverbDSP.hpp"
#include "AssistedReverbSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginAssistedReverb : public NullMultiPlugin
{
public:
    PluginAssistedReverb()
        : NullMultiPlugin(nullassistedreverb::kSliders, nullassistedreverb::kNumSliders)
    {
        for (uint32_t i = 0; i < nullassistedreverb::kNumSliders; ++i)
            fDsp.s[i] = nullassistedreverb::sliderValue(i, value(i));
        fDsp.setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "AssistedReverb"; }
    const char* getDescription() const override
    {
        return "A reverb that sets itself: Learn listens to the track, then decay, pre-delay, filters, level and ducking follow the chosen target.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'A', 'R', 'v'); }

    void valueChanged(uint32_t index, float v) override
    {
        fDsp.s[index] = nullassistedreverb::sliderValue(index, v);
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
            for (uint32_t i = 8; i < nullassistedreverb::kNumSliders; ++i)
                setOwnValue(i, float(fDsp.s[i]));
            taps.aux[1].store(taps.aux[1].load(std::memory_order_relaxed) + 1.0f, std::memory_order_release);
        }
        // for the interface: ducking (dB), listening or not, how far, and seconds heard
        taps.gr.store(float(fDsp.duckDb()), std::memory_order_relaxed);
        taps.aux[2].store(fDsp.learning() ? 1.0f : 0.0f, std::memory_order_relaxed);
        taps.extra.store(float(fDsp.progress()), std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.heard()), std::memory_order_relaxed);
    }

private:
    nullassistedreverb::AssistedReverbProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAssistedReverb)
};

Plugin* createPlugin()
{
    return new PluginAssistedReverb();
}

END_NAMESPACE_DISTRHO
