/*
 * NULL JSFX - Rotary Speaker (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "RotarySpeakerDSP.hpp"
#include "RotarySpeakerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginRotarySpeaker : public NullMultiPlugin
{
public:
    PluginRotarySpeaker()
        : NullMultiPlugin(nullrotary::kSliders, nullrotary::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "RotarySpeaker"; }
    const char* getDescription() const override
    {
        return "Leslie-style rotary speaker: horn and drum rotors with inertia, Doppler, tube drive and stereo mics.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '7', 'R', 's'); }

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
        // rotor phases and speeds for the display
        taps.extra.store(float(fDsp.hornPhase()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.drumPhase()), std::memory_order_relaxed);
        taps.inLevel.store(float(fDsp.hornHz()), std::memory_order_relaxed);
        taps.outLevel.store(float(fDsp.drumHz()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullrotary::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullrotary::RotaryProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRotarySpeaker)
};

Plugin* createPlugin()
{
    return new PluginRotarySpeaker();
}

END_NAMESPACE_DISTRHO
