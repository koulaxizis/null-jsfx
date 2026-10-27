/*
 * NULL JSFX - Phaser (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PhaserDSP.hpp"
#include "PhaserSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPhaser : public NullMultiPlugin
{
public:
    PluginPhaser()
        : NullMultiPlugin(nullphaser::kSliders, nullphaser::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Phaser"; }
    const char* getDescription() const override
    {
        return "Phaser: 1 to 8 allpass stages swept by a sine LFO around a centre frequency; rate (or tempo sync), stages, mix, depth, feedback, centre and stereo phase.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '4', 'p', 'h'); }

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
        // the host transport like pumper: tempo (else 120) and, while playing, the beat position
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
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo, tp.playing, beatPos);
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        nullphaser::applyParams(fDsp, [this](uint32_t i) { return value(i); });
    }

    nullphaser::PhaserProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPhaser)
};

Plugin* createPlugin()
{
    return new PluginPhaser();
}

END_NAMESPACE_DISTRHO
