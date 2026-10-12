/*
 * NULL JSFX - Spectrum Analyzer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "SpectrumAnalyzerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSpectrumAnalyzer : public SpectrumAnalyzerBase
{
public:
    PluginSpectrumAnalyzer()
    {
        fDsp.setView(&view);
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "SpectrumAnalyzer"; }
    const char* getDescription() const override
    {
        return "FFT spectrum analyzer: log-frequency display with slope, release, range and peak hold.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '0', 'S', 'a'); }

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
    }

private:
    void update()
    {
        fDsp.setParams(nullspec::fftSize(value(0)), value(2), int(std::lround(value(4))), int(std::lround(value(5))),
                       value(6) >= 0.5f);
    }

    nullspec::SpectrumAnalyzer fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpectrumAnalyzer)
};

Plugin* createPlugin()
{
    return new PluginSpectrumAnalyzer();
}

END_NAMESPACE_DISTRHO
