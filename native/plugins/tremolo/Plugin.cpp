/*
 * NULL JSFX - Tremolo (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "TremoloDSP.hpp"
#include "TremoloSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTremolo : public NullMultiPlugin
{
public:
    PluginTremolo()
        : NullMultiPlugin(nulltremolo::kSliders, nulltremolo::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Tremolo"; }
    const char* getDescription() const override
    {
        return "Tremolo: an LFO (sine, triangle, square or saw, free or tempo-synced) moves the level between full and 1 - depth; stereo phase for auto-pan and a harmonic mode.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 't', 'r'); }

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
        // the LFO position and the rate in use (the host's tempo when synced) for the display
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.rateHz()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nulltremolo::onGrid;
        fDsp.setParams(onGrid(value(0), 0.1, 20.0, 0.1), value(1), value(2), int(std::lround(value(3))),
                       int(std::lround(value(4))), value(5), value(6) >= 0.5f);
    }

    nulltremolo::TremoloProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTremolo)
};

Plugin* createPlugin()
{
    return new PluginTremolo();
}

END_NAMESPACE_DISTRHO
