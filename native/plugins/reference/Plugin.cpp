/*
 * NULL JSFX - Reference (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "ReferencePlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginReference : public ReferenceBase
{
public:
    PluginReference()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Reference"; }
    const char* getDescription() const override
    {
        return "Switches between the mix and a reference track on the sidechain, played at the mix's loudness, "
               "with both loudnesses, correlations and spectra side by side.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 'e', 'f'); }

    // inputs 1/2: the track (stereo), inputs 3/4: the reference (JSFX pins 3/4)
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input && index >= 2)
        {
            port.hints = kAudioPortIsSidechain;
            port.name = index == 2 ? "Reference Left" : "Reference Right";
            port.symbol = index == 2 ? "reference_left" : "reference_right";
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
            const double rl = inputs[2][i], rr = inputs[3][i];
            analyzer.push(float((l + r) * 0.5), float((rl + rr) * 0.5 * fDsp.referenceGain()));
            fDsp.processSample(l, r, rl, rr);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
        }
        trackLufs.store(float(fDsp.trackLufs()), std::memory_order_relaxed);
        referenceLufs.store(float(fDsp.referenceLufs()), std::memory_order_relaxed);
        referenceGain.store(float(fDsp.referenceGain()), std::memory_order_relaxed);
        trackCorr.store(float(fDsp.trackCorr()), std::memory_order_relaxed);
        referenceCorr.store(float(fDsp.referenceCorr()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullref::sliderValue(0, value(0)), nullref::sliderValue(1, value(1)),
                       nullref::sliderValue(2, value(2)), nullref::sliderValue(3, value(3)));
    }

    nullref::ReferenceProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginReference)
};

Plugin* createPlugin()
{
    return new PluginReference();
}

END_NAMESPACE_DISTRHO
