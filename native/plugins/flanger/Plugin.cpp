/*
 * NULL JSFX - Flanger (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "FlangerDSP.hpp"
#include "FlangerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginFlanger : public NullMultiPlugin
{
public:
    PluginFlanger()
        : NullMultiPlugin(nullflanger::kSliders, nullflanger::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Flanger"; }
    const char* getDescription() const override
    {
        return "Stereo flanger: a sine LFO sweeps a short delay line with feedback; rate, depth, feedback, mix, "
               "manual, tempo sync, stereo phase and thru-zero.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'f', 'l'); }

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
        taps.aux[1].store(float(fDsp.thruZeroDelay() * 1000.0 / getSampleRate()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nullflanger::onGrid;
        const int sync = int(std::lround(value(5)));
        fDsp.setParams(onGrid(value(0), 0.1, 5.0, 0.1), onGrid(value(1), 1.0, 20.0, 0.1), value(2), value(3),
                       onGrid(value(4), 0.1, 10.0, 0.1), sync, nullflanger::syncCycle(sync), std::round(value(6)),
                       value(7) >= 0.5f);
        setLatency(uint32_t(fDsp.latency())); // the JSFX's pdc_delay: the dry path's delay while Thru Zero is on
    }

    nullflanger::FlangerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFlanger)
};

Plugin* createPlugin()
{
    return new PluginFlanger();
}

END_NAMESPACE_DISTRHO
