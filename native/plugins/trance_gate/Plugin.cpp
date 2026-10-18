/*
 * NULL JSFX - Trance Gate (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "TranceGateDSP.hpp"
#include "TranceGateSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTranceGate : public NullMultiPlugin
{
public:
    PluginTranceGate()
        : NullMultiPlugin(nulltrance::kSliders, nulltrance::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        taps.aux[0].store(1.0f, std::memory_order_relaxed);
    }

protected:
    const char* getLabel() const override { return "TranceGate"; }
    const char* getDescription() const override
    {
        return "16-step rhythmic gate locked to the host tempo (or free-running), with attack, release, depth and a seeded per-step probability.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '7', 'T', 'g'); }

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
        // the current step for the display's playhead, and the tempo for the step duration
        taps.extra.store(float(fDsp.currentStep()), std::memory_order_relaxed);
        taps.gr.store(float(tempo), std::memory_order_relaxed);
        taps.aux[0].store(fDsp.stepKept() ? 1.0f : 0.0f, std::memory_order_relaxed); // Probability let the step play
    }

private:
    void update()
    {
        fDsp.setParams(nulltrance::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nulltrance::TranceGateProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTranceGate)
};

Plugin* createPlugin()
{
    return new PluginTranceGate();
}

END_NAMESPACE_DISTRHO
