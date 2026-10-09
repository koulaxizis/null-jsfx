/*
 * NULL JSFX - Louder (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "LouderDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginLouder : public NullOneSliderPlugin
{
public:
    PluginLouder()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        init(getSampleRate());
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "Louder"; }
    const char* getDescription() const override
    {
        return "Loudness maximizer: up to +12 dB of gain into a 2 ms look-ahead peak limiter at -0.3 dBFS.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'L', 'd', 'r'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // input meter first: the host may process in place
        for (uint32_t i = 0; i < frames; ++i)
            fInPeak.process(std::fmax(std::fabs(inputs[0][i]), std::fabs(inputs[1][i])));
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        for (uint32_t i = 0; i < frames; ++i)
            fOutPeak.process(std::fmax(std::fabs(outputs[0][i]), std::fabs(outputs[1][i])));
        taps.inLevel.store(fInPeak.value, std::memory_order_relaxed);
        taps.outLevel.store(fOutPeak.value, std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(fDsp.env(), 1e-7))), std::memory_order_relaxed);
    }

private:
    void init(double sr)
    {
        // @init (the JSFX's @slider follows; the gain does not depend on the rate)
        fDsp.setSampleRate(sr);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay = la
        fInPeak.setup(sr);
        fOutPeak.setup(sr);
    }

    nulllouder::LouderProcessor fDsp;
    nulljsfx::PeakFollower fInPeak, fOutPeak;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginLouder)
};

Plugin* createPlugin()
{
    return new PluginLouder();
}

END_NAMESPACE_DISTRHO
