/*
 * NULL JSFX - Humanize (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio passes through delayed by the latency (in line with the MIDI), the
 * MIDI goes through HumanizeProcessor
 * (humanize.jsfx's @block) with the host's transport (stopped or without one: a beat that runs
 * at the tempo, 120 bpm when the host gives none). The latency (the JSFX's pdc_delay) is
 * reported. SysEx, which the JSFX never sees, passes through at its frame. For the display: the
 * View (the last notes, the tempo, the latency).
 */

#include "NullPlugin.hpp"
#include "HumanizeDSP.hpp"
#include "HumanizeSliders.hpp"
#include "NullMidiTaps.hpp"

START_NAMESPACE_DISTRHO

class PluginHumanize : public HumanizeBase
{
public:
    PluginHumanize()
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Humanize"; }
    const char* getDescription() const override
    {
        return "MIDI effect: small random changes of timing and velocity plus a groove (swing 8ths or 16ths, laid back, push), repeatable from a seed, with the latency reported for delay compensation.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'H', 'u', 'm'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override { reset(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reset(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        double beat = 0.0;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beat = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) + tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beat = double(tp.frame) / getSampleRate() * tempo / 60.0;
        }

        fDsp.begin(tp.playing, tempo, beat);
        fDsp.audio(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        for (uint32_t i = 0; i < midiCount; ++i)
            if (midi[i].size >= 1 && midi[i].size <= 3)
                fDsp.input(midi[i].frame, midi[i].data[0], midi[i].size > 1 ? midi[i].data[1] : 0,
                           midi[i].size > 2 ? midi[i].data[2] : 0);
        nullmidi::sendMidi(*this, fDsp, frames, midi, midiCount);

        // the display: each note's beats in its bar
        const int n = fDsp.historyCount();
        for (int i = 0; i < n; ++i)
        {
            const nullhumanize::HistoryNote& h = fDsp.history(i);
            const double bar = 4.0 * std::floor(h.in / 4.0);
            view.in[i].store(float(h.in - bar), std::memory_order_relaxed);
            view.out[i].store(float(h.out - bar), std::memory_order_relaxed);
            view.vel[i].store(float(h.vel), std::memory_order_relaxed);
            view.velOut[i].store(float(h.velOut), std::memory_order_relaxed);
        }
        view.count.store(n, std::memory_order_relaxed);
        view.tempo.store(float(tempo), std::memory_order_relaxed);
    }

private:
    // @init then @slider
    void reset(double sr)
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
        update();
    }

    void update()
    {
        nullhumanize::Params p;
        p.timing = std::round(value(0));
        p.velocity = std::round(value(1));
        p.groove = int(std::lround(value(2)));
        p.amount = std::round(value(3));
        p.seed = std::round(value(4));
        fDsp.setParams(p);
        const double lat = fDsp.latency();
        setLatency(uint32_t(lat));
        view.latencyMs.store(float(lat * 1000.0 / getSampleRate()), std::memory_order_relaxed);
    }

    nullhumanize::HumanizeProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHumanize)
};

Plugin* createPlugin()
{
    return new PluginHumanize();
}

END_NAMESPACE_DISTRHO
