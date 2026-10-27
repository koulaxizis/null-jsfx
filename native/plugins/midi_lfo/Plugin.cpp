/*
 * NULL JSFX - MIDI LFO (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio passes through, the MIDI goes through MidiLfoProcessor
 * (midi_lfo.jsfx's @block) with the host's transport (120 bpm when the host gives no tempo).
 * SysEx, which the JSFX never sees, passes through at its frame. For the display: extra = the
 * phase (0..1), inLevel = the value (0..1), outLevel / gr = the random values the cycle goes
 * from and to, aux[0] = 1 while the host plays, aux[1] = the tempo.
 */

#include "NullPlugin.hpp"
#include "MidiLfoDSP.hpp"
#include "MidiLfoSliders.hpp"
#include "NullMidiTaps.hpp"

START_NAMESPACE_DISTRHO

class PluginMidiLfo : public NullMultiPlugin
{
public:
    PluginMidiLfo()
        : NullMultiPlugin(nullmidilfo::kSliders, nullmidilfo::kNumSliders)
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "MIDI LFO"; }
    const char* getDescription() const override
    {
        return "MIDI effect: an LFO (sine, triangle, saws, square, random, smooth random) that sends a CC, pitch bend or channel pressure, free in Hz or locked to the host's tempo.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'l', 'f'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { reset(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reset(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (int c = 0; c < 2; ++c)
            if (outputs[c] != inputs[c])
                std::memmove(outputs[c], inputs[c], sizeof(float) * frames);

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

        fDsp.begin(frames, tp.playing, tempo, beat);
        for (uint32_t i = 0; i < midiCount; ++i)
            if (midi[i].size >= 1 && midi[i].size <= 3)
                fDsp.input(midi[i].frame, midi[i].data[0], midi[i].size > 1 ? midi[i].data[1] : 0,
                           midi[i].size > 2 ? midi[i].data[2] : 0);
        nullmidi::sendMidi(*this, fDsp, frames, midi, midiCount);

        taps.extra.store(float(fDsp.outPhase()), std::memory_order_relaxed);
        taps.inLevel.store(float(fDsp.outValue()), std::memory_order_relaxed);
        taps.outLevel.store(float(fDsp.randomPrev()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.randomNext()), std::memory_order_relaxed);
        taps.aux[0].store(tp.playing ? 1.0f : 0.0f, std::memory_order_relaxed);
        taps.aux[1].store(float(tempo), std::memory_order_relaxed);
    }

private:
    // @init then @slider
    void reset(double sr)
    {
        fDsp.setSampleRate(sr);
        fDsp.reset();
        update();
    }

    int iv(uint32_t i) const { return int(std::lround(value(i))); }

    void update()
    {
        using namespace nullmidilfo;
        Params p;
        p.shape = iv(0);
        p.sync = iv(1);
        p.rate = jsfxValue(kSliders[2], value(2));
        p.length = kLengthBeats[std::max(0, std::min(15, iv(3)))];
        p.phase = std::round(value(4)) / 360.0;
        p.dest = iv(5);
        p.cc = iv(6);
        p.channel = iv(7);
        p.min = std::round(value(8)) / 100.0;
        p.max = std::round(value(9)) / 100.0;
        p.retrigger = iv(10);
        p.seed = iv(11);
        fDsp.setParams(p);
    }

    nullmidilfo::MidiLfoProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMidiLfo)
};

Plugin* createPlugin()
{
    return new PluginMidiLfo();
}

END_NAMESPACE_DISTRHO
