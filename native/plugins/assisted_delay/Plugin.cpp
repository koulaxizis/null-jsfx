/*
 * NULL JSFX - Assisted Delay (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "AssistedDelayDSP.hpp"
#include "AssistedDelaySliders.hpp"

START_NAMESPACE_DISTRHO

class PluginAssistedDelay : public NullMultiPlugin
{
public:
    PluginAssistedDelay()
        : NullMultiPlugin(nullassisteddelay::kSliders, nullassisteddelay::kNumSliders)
    {
        for (uint32_t i = 0; i < nullassisteddelay::kNumSliders; ++i)
            fDsp.s[i] = nullassisteddelay::sliderValue(i, value(i));
        fDsp.setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "AssistedDelay"; }
    const char* getDescription() const override
    {
        return "A delay that sets itself: Learn listens to the track, then time, repeats, filters, level and ducking follow the chosen target.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'A', 'D', 'l'); }

    void valueChanged(uint32_t index, float v) override
    {
        fDsp.s[index] = nullassisteddelay::sliderValue(index, v);
        fDsp.update();
    }

    void onActivate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        bool learned = fDsp.block(tempo);
        learned = fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames) || learned;
        if (learned)
        {
            // Learn wrote its sliders (and set Learn back to Off), as the JSFX does; the
            // interface sees the count go up and tells the host
            setOwnValue(0, float(fDsp.s[0]));
            for (uint32_t i = 8; i < nullassisteddelay::kNumSliders; ++i)
                setOwnValue(i, float(fDsp.s[i]));
            taps.aux[1].store(taps.aux[1].load(std::memory_order_relaxed) + 1.0f, std::memory_order_release);
        }
        // for the interface: ducking (dB), listening or not, how far, seconds heard and the tempo
        taps.gr.store(float(fDsp.duckDb()), std::memory_order_relaxed);
        taps.aux[2].store(fDsp.learning() ? 1.0f : 0.0f, std::memory_order_relaxed);
        taps.extra.store(float(fDsp.progress()), std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.heard()), std::memory_order_relaxed);
        taps.aux[3].store(float(tempo), std::memory_order_relaxed);
    }

private:
    nullassisteddelay::AssistedDelayProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAssistedDelay)
};

Plugin* createPlugin()
{
    return new PluginAssistedDelay();
}

END_NAMESPACE_DISTRHO
