/*
 * NULL JSFX - Tuner (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "TunerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTuner : public TunerBase
{
public:
    PluginTuner()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Tuner"; }
    const char* getDescription() const override
    {
        return "Chromatic tuner with needle and strobe displays, A4 from 415 to 466 Hz, and a mute for tuning on stage.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'T', 'u', 'n'); }

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
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        view.freq.store(float(fDsp.frequency()), std::memory_order_relaxed);
    }

private:
    void update() { fDsp.setParams(value(2), value(3)); }

    nulltuner::Tuner fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTuner)
};

Plugin* createPlugin()
{
    return new PluginTuner();
}

END_NAMESPACE_DISTRHO
