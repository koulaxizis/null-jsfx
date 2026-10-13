/*
 * NULL JSFX - Delay (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DelayDSP.hpp"
#include "DelaySliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDelay : public NullMultiPlugin
{
public:
    PluginDelay()
        : NullMultiPlugin(nulldelay::kSliders, nulldelay::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Delay"; }
    const char* getDescription() const override
    {
        return "Stereo echo and slapback: time (or a tempo-synced note), feedback and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'l', 'y'); }

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
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo);
        taps.extra.store(float(fDsp.delayMsNow()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3) >= 0.5f, int(std::lround(value(4))));
    }

    nulldelay::DelayProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDelay)
};

Plugin* createPlugin()
{
    return new PluginDelay();
}

END_NAMESPACE_DISTRHO
