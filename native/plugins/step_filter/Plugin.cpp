/*
 * NULL JSFX - Step Filter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "StepFilterDSP.hpp"
#include "StepFilterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginStepFilter : public NullMultiPlugin
{
public:
    PluginStepFilter()
        : NullMultiPlugin(nullstepfilter::kSliders, nullstepfilter::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "StepFilter"; }
    const char* getDescription() const override
    {
        return "Filter whose cutoff follows a 16-step sequence locked to the host tempo, with glide, resonance and seeded generated steps.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 't', 'p'); }

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
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        double beatPos = 0.0;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beatPos = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) +
                          tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beatPos = double(tp.frame) / getSampleRate() * tempo / 60.0;
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo, tp.playing, beatPos);
        // the current step for the display's playhead, and the tempo
        taps.extra.store(float(fDsp.currentStep()), std::memory_order_relaxed);
        taps.gr.store(float(tempo), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullstepfilter::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullstepfilter::StepFilterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStepFilter)
};

Plugin* createPlugin()
{
    return new PluginStepFilter();
}

END_NAMESPACE_DISTRHO
