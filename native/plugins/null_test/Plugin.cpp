/*
 * NULL JSFX - Null Test (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * NullTest (null_test.jsfx's @sample): the main (inputs 1/2) against the sidechain (inputs
 * 3/4), lined up in delay and gain. For the display: extra = the null depth (dB; -300 the
 * difference is zero, -400 the main is silent), gr = the delay (samples), aux[0..9] = the
 * difference per octave band (dB), aux[10] = the gain, aux[11] = the match, aux[12] = 1 when
 * the difference has been exactly zero for a second.
 */

#include "NullPlugin.hpp"
#include "NullTestDSP.hpp"
#include "NullTestSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginNullTest : public NullMultiPlugin
{
public:
    PluginNullTest()
        : NullMultiPlugin(nullnulltest::kSliders, nullnulltest::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "NullTest"; }
    const char* getDescription() const override
    {
        return "Plays and measures the difference between the main input and the sidechain, lined up in delay and gain: "
               "the null depth, the difference per band, or bit-identical.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'N', 'l', 'T'); }

    // inputs 1/2: the main (stereo), inputs 3/4: the sidechain (JSFX pins 3/4)
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
        taps.extra.store(float(fDsp.depth()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.delay()), std::memory_order_relaxed);
        for (int i = 0; i < nullnulltest::NullTest::kBands; ++i)
            taps.aux[i].store(float(fDsp.band(i)), std::memory_order_relaxed);
        taps.aux[10].store(float(fDsp.gain()), std::memory_order_relaxed);
        taps.aux[11].store(float(fDsp.confidence()), std::memory_order_relaxed);
        taps.aux[12].store(fDsp.identical() ? 1.0f : 0.0f, std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullnulltest::paramsFrom([this](uint32_t i) { return nullnulltest::sliderValue(i, value(i)); }));
    }

    nullnulltest::NullTest fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginNullTest)
};

Plugin* createPlugin()
{
    return new PluginNullTest();
}

END_NAMESPACE_DISTRHO
