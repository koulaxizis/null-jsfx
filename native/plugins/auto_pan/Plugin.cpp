/*
 * NULL JSFX - Auto Pan (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "AutoPanDSP.hpp"
#include "AutoPanSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginAutoPan : public NullMultiPlugin
{
public:
    PluginAutoPan()
        : NullMultiPlugin(nullautopan::kSliders, nullautopan::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "AutoPan"; }
    const char* getDescription() const override
    {
        return "Auto-panner: a sine LFO or stepped pan patterns at a rate (or a tempo-synced note), with depth, mix and step smoothing.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'a', 'p'); }

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
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.rate()), std::memory_order_relaxed); // the LFO rate in Hz, not a gain reduction
        taps.aux[0].store(float(fDsp.step()), std::memory_order_relaxed); // the step (step modes)
    }

private:
    void update()
    {
        using nullautopan::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 10.0, 0.1), value(1), value(2), value(3) >= 0.5f, int(std::lround(value(4))),
                       int(std::lround(value(5))), std::round(value(6)));
    }

    nullautopan::AutoPanProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAutoPan)
};

Plugin* createPlugin()
{
    return new PluginAutoPan();
}

END_NAMESPACE_DISTRHO
