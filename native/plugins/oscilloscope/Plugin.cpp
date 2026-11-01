/*
 * NULL JSFX - Oscilloscope (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "OscilloscopeSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginOscilloscope : public OscilloscopeBase
{
public:
    PluginOscilloscope()
    {
        fDsp.setView(&view);
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Oscilloscope"; }
    const char* getDescription() const override
    {
        return "Waveform display with trigger, time or tempo-synced width, mid, left/right or side view and hold; the audio passes through untouched.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'O', 's', 'c'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo);
    }

private:
    void update()
    {
        fDsp.setParams(nullosc::paramsFrom([this](uint32_t i) { return nullosc::sliderValue(i, value(i)); }));
    }

    nullosc::Oscilloscope fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginOscilloscope)
};

Plugin* createPlugin()
{
    return new PluginOscilloscope();
}

END_NAMESPACE_DISTRHO
