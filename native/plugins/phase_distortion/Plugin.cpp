/*
 * NULL JSFX - Phase Distortion (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PhaseDistortionDSP.hpp"
#include "PhaseDistortionSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPhaseDistortion : public NullMultiPlugin
{
public:
    PluginPhaseDistortion()
        : NullMultiPlugin(nullphasedist::kSliders, nullphasedist::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "PhaseDistortion"; }
    const char* getDescription() const override
    {
        return "Phase distortion: a very short delay moved by the track itself or the sidechain; drive, tone, bias, spread and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'h', 'D'); }

    // inputs 1/2: the track (stereo), inputs 3/4: the sidechain (JSFX pins 3/4)
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
        fDsp.process(inputs[0], inputs[1], inputs[2], inputs[3], outputs[0], outputs[1], frames);
    }

private:
    void update()
    {
        using nullphasedist::kSliders;
        double v[nullphasedist::kNumSliders];
        for (uint32_t i = 0; i < nullphasedist::kNumSliders; ++i)
            v[i] = jsfxValue(kSliders[i], value(i));
        fDsp.setParams(nullphasedist::paramsFor(v[0], v[1], v[2], v[3], v[4], v[5], v[6], getSampleRate()));
    }

    nullphasedist::PhaseDistortionProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPhaseDistortion)
};

Plugin* createPlugin()
{
    return new PluginPhaseDistortion();
}

END_NAMESPACE_DISTRHO
