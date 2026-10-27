/*
 * NULL JSFX - Filter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "FilterDSP.hpp"
#include "FilterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginFilter : public NullMultiPlugin
{
public:
    PluginFilter()
        : NullMultiPlugin(nullfilter::kSliders, nullfilter::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Filter"; }
    const char* getDescription() const override
    {
        return "Resonant state-variable filter: low pass, high pass, band pass or notch at 12 or 24 dB/oct, with drive, output gain, a free or tempo-synced LFO and an envelope follower on the cutoff.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '2', 'F', 'L'); }

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
        // the cutoff the filter ends the block on, for the display
        taps.extra.store(float(fDsp.modulatedCutoff()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        nullfilter::Params p;
        p.cutoff = value(0);
        p.resonance = value(1);
        p.gain = value(2);
        p.mode = int(std::lround(value(3)));
        p.slope = int(std::lround(value(4)));
        p.drive = value(5);
        p.lfoRate = value(6);
        p.lfoSync = int(std::lround(value(7)));
        p.lfoDepth = value(8);
        p.envDepth = value(9);
        p.envAttack = value(10);
        p.envRelease = value(11);
        fDsp.setParams(p);
    }

    nullfilter::FilterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFilter)
};

Plugin* createPlugin()
{
    return new PluginFilter();
}

END_NAMESPACE_DISTRHO
