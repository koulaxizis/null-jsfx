/*
 * NULL JSFX - Chords (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio passes through, the MIDI goes through ChordsProcessor (chords.jsfx's
 * @block). SysEx, which the JSFX never sees, passes through at its frame. For the display:
 * aux[0..7] = the output notes sounding (16 notes per value, as bits), aux[8..15] = the keys
 * held, extra = 1 + the last key that played a chord (0 = none yet).
 */

#include "NullPlugin.hpp"
#include "ChordsDSP.hpp"
#include "ChordsSliders.hpp"
#include "NullMidiTaps.hpp"

START_NAMESPACE_DISTRHO

class PluginChords : public NullMultiPlugin
{
public:
    PluginChords()
        : NullMultiPlugin(nullchords::kSliders, nullchords::kNumSliders)
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Chords"; }
    const char* getDescription() const override
    {
        return "MIDI effect: one key plays a chord of the chosen key and scale (triad, 7th, 9th, sus2, sus4 or power, with inversion, voicing, strum and a velocity curve), or the white keys play the steps of a chord progression.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'C', 'h', 'd'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override { reset(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reset(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (int c = 0; c < 2; ++c)
            if (outputs[c] != inputs[c])
                std::memmove(outputs[c], inputs[c], sizeof(float) * frames);
        fDsp.begin();
        for (uint32_t i = 0; i < midiCount; ++i)
            if (midi[i].size >= 1 && midi[i].size <= 3)
                fDsp.input(midi[i].frame, midi[i].data[0], midi[i].size > 1 ? midi[i].data[1] : 0,
                           midi[i].size > 2 ? midi[i].data[2] : 0);
        nullmidi::sendMidi(*this, fDsp, frames, midi, midiCount);
        nullmidi::midiTaps(taps, fDsp.notes());
        taps.extra.store(float(fDsp.lastNote() + 1), std::memory_order_relaxed);
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
        nullchords::Params p;
        p.mode = int(std::lround(value(0)));
        p.key = int(std::lround(value(1)));
        p.scale = int(std::lround(value(2)));
        p.type = int(std::lround(value(3)));
        p.inv = int(std::lround(value(4)));
        p.voicing = int(std::lround(value(5)));
        p.octave = int(std::lround(value(6)));
        p.strumMs = std::round(value(7));
        p.dir = int(std::lround(value(8)));
        p.curve = std::round(value(9));
        p.prog = int(std::lround(value(10)));
        fDsp.setParams(p);
    }

    nullchords::ChordsProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChords)
};

Plugin* createPlugin()
{
    return new PluginChords();
}

END_NAMESPACE_DISTRHO
