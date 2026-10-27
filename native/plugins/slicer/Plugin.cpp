/*
 * NULL JSFX - Slicer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SlicerDSP.hpp"
#include "SlicerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSlicer : public NullMultiPlugin
{
public:
    PluginSlicer()
        : NullMultiPlugin(nullslicer::kSliders, nullslicer::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Slicer"; }
    const char* getDescription() const override
    {
        return "Real-time loop remixer locked to the host tempo: replays the previous bar cut into 8, 16 or 32 slices in a shuffled, reversed, stuttered, backspun or random order, with repeat and reverse chances, gate and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'l', 'c'); }

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
            // REAPER re-runs the JSFX's @init when the transport starts
            if (!fWasPlaying)
                fDsp.reset();
        }
        fWasPlaying = tp.playing;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo, tp.playing, beatPos);
        // the playhead (position in the bar) and whether a whole bar has been recorded
        taps.extra.store(float(fDsp.barPos()), std::memory_order_relaxed);
        taps.aux[0].store(fDsp.full() ? 1.0f : 0.0f, std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullslicer::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullslicer::SlicerProcessor fDsp;
    bool fWasPlaying = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSlicer)
};

Plugin* createPlugin()
{
    return new PluginSlicer();
}

END_NAMESPACE_DISTRHO
