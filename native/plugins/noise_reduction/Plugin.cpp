/*
 * NULL JSFX - Noise Reduction (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NoiseReductionSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginNoiseReduction : public NoiseReductionBase
{
public:
    PluginNoiseReduction()
    {
        fDsp.setView(&view);
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "NoiseReduction"; }
    const char* getDescription() const override
    {
        return "Stationary noise reducer: learns a noise profile or tracks the noise floor, Wiener-like spectral gain.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'N', 'R', 'd'); }

    void valueChanged(uint32_t, float) override { update(); }

    // like the JSFX's @init: the learnt profile survives unless the sample rate changes
    void onActivate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        setLatency(uint32_t(nullnr::NoiseReducer::latency())); // pdc_delay = N - 1
        update();
    }

    void update()
    {
        // Threshold as the JSFX slider holds it (min + k * 0.5 in double)
        const double thr = -10.0 + std::round((double(value(3)) + 10.0) / 0.5) * 0.5;
        fDsp.setParams(value(0) >= 0.5f, value(1) >= 0.5f, std::round(value(2)), thr, std::round(value(4)),
                       std::round(value(5)), std::round(value(6)), value(7) >= 0.5f, std::round(value(8)));
    }

    nullnr::NoiseReducer fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginNoiseReduction)
};

Plugin* createPlugin()
{
    return new PluginNoiseReduction();
}

END_NAMESPACE_DISTRHO
