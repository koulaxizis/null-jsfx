/*
 * NULL JSFX - Dynamics (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in dynamics_gfx.jsfx): inLevel / outLevel = input / output peak
 * meters, gr = gain reduction in dB. inLevel is the detector's level (the sidechain when External).
 */

#include "NullPlugin.hpp"
#include "DynamicsDSP.hpp"
#include "DynamicsSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDynamics : public NullMultiPlugin
{
public:
    PluginDynamics()
        : NullMultiPlugin(nulldynamics::kSliders, nulldynamics::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Dynamics"; }
    const char* getDescription() const override
    {
        return "Compressor or downward expander with threshold, ratio, soft knee, attack, release, makeup, four "
               "detector styles, a sidechain high-pass, parallel mix and an external sidechain.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'd', 'y'); }

    // inputs 1/2: the track (the main stereo bus, as before), inputs 3/4: a separate sidechain
    // port (JSFX pins 3/4), as in plugins/sidechain_ducker
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
        setup(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        setup(newSampleRate);
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
            fIn.process(float(fDsp.inputAbs()));
            fOut.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(fDsp.gain(), 0.0000001))), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
    }

    void update()
    {
        fDsp.setParams(int(std::lround(value(0))), value(1), value(2), value(3), value(4), value(5), value(6),
                       int(std::lround(value(7))), value(8), value(9), value(10) > 0.5f);
    }

    nulldynamics::DynamicsProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDynamics)
};

Plugin* createPlugin()
{
    return new PluginDynamics();
}

END_NAMESPACE_DISTRHO
