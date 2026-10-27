/*
 * NULL JSFX - Wah (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The host tempo (120 when it reports none) and, while the transport plays, its beat position
 * drive the synced LFO, as the JSFX's tempo / play_state / beat_position. The live pedal
 * position goes to the display through taps.extra.
 */

#include "NullPlugin.hpp"
#include "WahDSP.hpp"
#include "WahSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginWah : public NullMultiPlugin
{
public:
    PluginWah()
        : NullMultiPlugin(nullwah::kSliders, nullwah::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Wah"; }
    const char* getDescription() const override
    {
        return "Wah pedal: a resonant band-pass swept by the pedal (for automation or a MIDI CC), an envelope follower (auto-wah) or a free or tempo-synced LFO, with Classic, Vocal and Wide curves.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'W', 'a', 'h'); }

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
        taps.extra.store(float(fDsp.pedalNow()), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nullwah::sliderValue(i, value(i)); }

    void update()
    {
        nullwah::Params p;
        p.pedal = v(0);
        p.mode = v(1);
        p.model = v(2);
        p.resonance = v(3);
        p.sensitivity = v(4);
        p.attack = v(5);
        p.release = v(6);
        p.rate = v(7);
        p.sync = v(8);
        p.mix = v(9);
        p.output = v(10);
        fDsp.setParams(p);
    }

    nullwah::WahProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWah)
};

Plugin* createPlugin()
{
    return new PluginWah();
}

END_NAMESPACE_DISTRHO
