/*
 * NULL JSFX - Sidechain Ducker (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SidechainDuckerDSP.hpp"
#include "SidechainDuckerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSidechainDucker : public NullMultiPlugin
{
public:
    PluginSidechainDucker()
        : NullMultiPlugin(nullducker::kSliders, nullducker::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "SidechainDucker"; }
    const char* getDescription() const override
    {
        return "Ducks the track when the sidechain key (or the track itself) goes above the threshold.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '9', 'S', 'd'); }

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
        fDsp.process(inputs[0], inputs[1], inputs[2], inputs[3], outputs[0], outputs[1], frames);
        taps.inLevel.store(float(fDsp.env()), std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(fDsp.gain(), 1e-7))), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4), value(5) >= 0.5f, value(6));
    }

    nullducker::DuckerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSidechainDucker)
};

Plugin* createPlugin()
{
    return new PluginSidechainDucker();
}

END_NAMESPACE_DISTRHO
