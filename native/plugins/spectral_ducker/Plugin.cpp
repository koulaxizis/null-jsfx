/*
 * NULL JSFX - Spectral Ducker (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "SpectralDuckerPlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginSpectralDucker : public SpectralDuckerBase
{
public:
    PluginSpectralDucker()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "SpectralDucker"; }
    const char* getDescription() const override
    {
        return "Makes room for the sidechain key inside the track, band by band: 32 bands follow the key and cut "
               "the same frequencies of the track while it plays.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'D', 'k'); }

    // inputs 1/2: the track (stereo), inputs 3/4: the sidechain key (JSFX pins 3/4)
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
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
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
        }
        for (int k = 0; k < nullsduck::kBands; ++k)
        {
            bandGr[k].store(float(fDsp.gr(k)), std::memory_order_relaxed);
            bandKey[k].store(float(fDsp.keyLevel(k)), std::memory_order_relaxed);
        }
    }

private:
    void update()
    {
        double v[nullsduck::kNumSliders];
        for (uint32_t i = 0; i < nullsduck::kNumSliders; ++i)
            v[i] = nullsduck::sliderValue(i, value(i));
        fDsp.setParams(v);
    }

    nullsduck::SpectralDuckerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpectralDucker)
};

Plugin* createPlugin()
{
    return new PluginSpectralDucker();
}

END_NAMESPACE_DISTRHO
