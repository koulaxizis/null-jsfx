/*
 * NULL JSFX - Phase Align (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Inputs 1/2: the track, inputs 3/4: the sidechain reference (JSFX pins 3/4). Reports the
 * JSFX's pdc_delay as latency: ceil(10 ms) + 1 samples with Source = Sidechain, else 0.
 * For the display: aux[0] / aux[1] = the correlation of the moved signal with its reference,
 * full band and below 150 Hz.
 */

#include "NullPlugin.hpp"
#include "PhaseAlignDSP.hpp"
#include "PhaseAlignSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPhaseAlign : public NullMultiPlugin
{
public:
    PluginPhaseAlign()
        : NullMultiPlugin(nullphase::kSliders, nullphase::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "PhaseAlign"; }
    const char* getDescription() const override
    {
        return "Lines up two sources in time, polarity and phase: the right channel to the left, or the track to a sidechain reference.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'h', 'A'); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input && index >= 2)
        {
            port.hints = kAudioPortIsSidechain;
            port.name = index == 2 ? "Sidechain Left" : "Sidechain Right";
            port.symbol = index == 2 ? "sidechain_left" : "sidechain_right";
            return;
        }
        port.groupId = kPortGroupStereo;
        NullMultiPlugin::initAudioPort(input, index, port);
    }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        setup(newSampleRate);
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r, inputs[2][i], inputs[3][i]);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            fCorr.push(fDsp.ca(), fDsp.cb());
        }
        taps.aux[0].store(float(fCorr.full()), std::memory_order_relaxed);
        taps.aux[1].store(float(fCorr.low()), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fCorr.setup(sr > 0.0 ? sr : 48000.0);
    }

    void update()
    {
        nullphase::Params p;
        p.sidechain = nullphase::sliderValue(0, value(0)) >= 0.5;
        p.delayMs = nullphase::sliderValue(1, value(1));
        p.fine = nullphase::sliderValue(2, value(2));
        p.invL = nullphase::sliderValue(3, value(3)) >= 0.5;
        p.invR = nullphase::sliderValue(4, value(4)) >= 0.5;
        p.rotation = nullphase::sliderValue(5, value(5));
        p.focus = nullphase::sliderValue(6, value(6));
        p.sum = nullphase::sliderValue(7, value(7)) >= 0.5;
        fDsp.setParams(p);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay
    }

    nullphase::PhaseAlignProcessor fDsp;
    nullphase::Correlation fCorr;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPhaseAlign)
};

Plugin* createPlugin()
{
    return new PluginPhaseAlign();
}

END_NAMESPACE_DISTRHO
