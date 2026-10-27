/*
 * NULL JSFX - Arpeggiator (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio passes through, the MIDI goes through ArpProcessor
 * (arpeggiator.jsfx's @block) with the host's transport (stopped or without one: free-running
 * at the tempo, 120 bpm when the host gives none). SysEx, which the JSFX never sees, passes
 * through at its frame. For the display: aux[0..7] = the output notes sounding (16 notes per
 * value, as bits), aux[8..15] = the notes in the arp (held or latched), extra = 1 + the step
 * playing (0 = none), gr = the tempo, inLevel = 1 while the host plays.
 */

#include "NullPlugin.hpp"
#include "ArpeggiatorDSP.hpp"
#include "ArpeggiatorSliders.hpp"
#include "NullMidiTaps.hpp"

START_NAMESPACE_DISTRHO

class PluginArpeggiator : public NullMultiPlugin
{
public:
    PluginArpeggiator()
        : NullMultiPlugin(nullarp::kSliders, nullarp::kNumSliders)
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Arpeggiator"; }
    const char* getDescription() const override
    {
        return "MIDI effect: plays the held notes one after another in time with the host (up, down, up-down, down-up, as played, random or as chords) over up to four octaves, with gate, swing, latch and a 16-step velocity lane.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'A', 'r', 'p'); }

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

        for (int g = 0; g < 8; ++g)
        {
            uint32_t on = 0;
            for (int i = 0; i < 16; ++i)
                for (int ch = 0; ch < 16; ++ch)
                    if (fDsp.sounding(ch * 128 + g * 16 + i))
                        on |= 1u << i;
            uint32_t held = 0;
            for (int i = 0; i < fDsp.noteCount(); ++i)
                if (fDsp.note(i) / 16 == g)
                    held |= 1u << (fDsp.note(i) % 16);
            taps.aux[g].store(float(on), std::memory_order_relaxed);
            taps.aux[8 + g].store(float(held), std::memory_order_relaxed);
        }
        taps.extra.store(float(fDsp.currentStep() + 1), std::memory_order_relaxed);
        taps.gr.store(float(tempo), std::memory_order_relaxed);
        taps.inLevel.store(tp.playing ? 1.0f : 0.0f, std::memory_order_relaxed);
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
        nullarp::Params p;
        p.rate = int(std::lround(value(0)));
        p.pattern = int(std::lround(value(1)));
        p.octaves = int(std::lround(value(2)));
        p.gate = std::round(value(3));
        p.swing = std::round(value(4));
        p.latch = int(std::lround(value(5)));
        p.steps = std::round(value(6));
        p.seed = std::round(value(7));
        for (int k = 0; k < nullarp::kLaneSteps; ++k)
            p.lane[k] = std::round(value(uint32_t(8 + k)));
        fDsp.setParams(p);
    }

    nullarp::ArpProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginArpeggiator)
};

Plugin* createPlugin()
{
    return new PluginArpeggiator();
}

END_NAMESPACE_DISTRHO
