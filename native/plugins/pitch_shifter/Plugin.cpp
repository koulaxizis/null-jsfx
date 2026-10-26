/*
 * NULL JSFX - Pitch Shifter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PitchShifterDSP.hpp"
#include "PitchShifterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPitchShifter : public NullMultiPlugin
{
public:
    PluginPitchShifter()
        : NullMultiPlugin(nullpitchshifter::kSliders, nullpitchshifter::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "PitchShifter"; }
    const char* getDescription() const override
    {
        return "Pitch shifter: two crossfaded read taps in a 60 ms delay line; octave, semitones, fine and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'p', 's'); }

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
        taps.extra.store(float(fDsp.tapPhase()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nullpitchshifter::onGrid;
        fDsp.setParams(value(0), value(1), onGrid(value(2), -50.0, 50.0, 0.1), value(3));
    }

    nullpitchshifter::PitchShifterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPitchShifter)
};

Plugin* createPlugin()
{
    return new PluginPitchShifter();
}

END_NAMESPACE_DISTRHO
