/*
 * NULL JSFX - Studio EQ (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reports the linear-phase latency (the JSFX's pdc_delay, 5120 samples, 0 at zero latency). For
 * the display: the mono input and output go into the analyzer ring (EqAnalyzer.hpp).
 */

#include "NullPlugin.hpp"
#include "StudioEqDSP.hpp"
#include "StudioEqPlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginStudioEq : public StudioEqBase
{
public:
    PluginStudioEq()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "StudioEQ"; }
    const char* getDescription() const override
    {
        return "Eight free EQ bands from 10 Hz to 30 kHz: bells, shelves, cuts up to 96 dB/oct, notch and band pass, "
               "each on stereo, mid, side, left or right, with auto gain and a linear-phase mode.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'E', 'q'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        analyzer.sampleRate.store(float(getSampleRate()), std::memory_order_relaxed);
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
        analyzer.sampleRate.store(float(newSampleRate), std::memory_order_relaxed);
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            const float pre = float((l + r) * 0.5);
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            analyzer.push(pre, float((l + r) * 0.5));
        }
    }

private:
    void update()
    {
        fDsp.setParams(nullseq::params([this](uint32_t i) { return nullseq::sliderValue(i, value(i)); }));
        setLatency(fDsp.latency()); // pdc_delay
    }

    nullseq::StudioEqProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStudioEq)
};

Plugin* createPlugin()
{
    return new PluginStudioEq();
}

END_NAMESPACE_DISTRHO
