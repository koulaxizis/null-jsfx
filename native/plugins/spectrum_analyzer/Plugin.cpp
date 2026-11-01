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
        return "FFT spectrum analyzer: log-frequency display or spectrogram with slope, release, range, peak hold and a sidechain overlay.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '0', 'S', 'a'); }

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

    // inputs 1/2: the track, inputs 3/4: the sidechain (JSFX pins 3/4), analysed with Sidechain On
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
        SpectrumAnalyzerBase::initAudioPort(input, index, port);
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, inputs[2], inputs[3]);
    }

private:
    void update()
    {
        fDsp.setParams(nullspec::fftSize(value(0)), value(2), int(std::lround(value(4))), int(std::lround(value(5))),
                       value(6) >= 0.5f, value(8) >= 0.5f);
    }

    nullspec::SpectrumAnalyzer fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpectrumAnalyzer)
};

Plugin* createPlugin()
{
    return new PluginSpectrumAnalyzer();
}

END_NAMESPACE_DISTRHO
