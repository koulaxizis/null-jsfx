/*
 * NULL JSFX - Chorus (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ChorusDSP.hpp"
#include "ChorusSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginChorus : public NullMultiPlugin
{
public:
    PluginChorus()
        : NullMultiPlugin(nullchorus::kSliders, nullchorus::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Chorus"; }
    const char* getDescription() const override
    {
        return "Stereo chorus: sine LFOs sweep up to 4 voices on a 5 ms + depth delay line, or a Dimension-style "
               "pair; rate, depth, mix, spread, feedback, tempo sync and high cut.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'c', 'h'); }

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
        // as pumper: the host tempo when above 0, else 120; the beat position only while playing
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        double beatPos = 0.0;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beatPos = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) +
                          tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beatPos = double(tp.frame) / getSampleRate() * tempo / 60.0;
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tp.playing, beatPos, tempo);
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.rateHz()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nullchorus::onGrid;
        const int sync = int(std::lround(value(6)));
        fDsp.setParams(onGrid(value(0), 0.1, 20.0, 0.1), onGrid(value(1), 0.0, 10.0, 0.1), value(2),
                       std::round(value(3)), std::round(value(4)), std::round(value(5)), sync,
                       nullchorus::syncCycle(sync), std::round(value(7)), value(8) >= 0.5f);
    }

    nullchorus::ChorusProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChorus)
};

Plugin* createPlugin()
{
    return new PluginChorus();
}

END_NAMESPACE_DISTRHO
