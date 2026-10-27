/*
 * NULL JSFX - MIDI Utility (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio passes through, the MIDI goes through MidiUtilityProcessor
 * (midi_utility.jsfx's @block). SysEx, which the JSFX never sees, passes through at its frame.
 * For the display: aux[0..7] = the output notes sounding (16 notes per value, as bits),
 * aux[8..15] = the keys held, extra = 1 + the last key played (0 = none yet), inLevel = 1 + the
 * note it played (0 = none: out of range, or nothing yet), outLevel = its velocity in, gr = the
 * velocity out.
 */

#include "NullPlugin.hpp"
#include "MidiUtilityDSP.hpp"
#include "MidiUtilitySliders.hpp"
#include "NullMidiTaps.hpp"

START_NAMESPACE_DISTRHO

class PluginMidiUtility : public NullMultiPlugin
{
public:
    PluginMidiUtility()
        : NullMultiPlugin(nullmidiutil::kSliders, nullmidiutil::kNumSliders)
    {
        reset();
    }

protected:
    const char* getLabel() const override { return "MIDI Utility"; }
    const char* getDescription() const override
    {
        return "MIDI effect: transpose, output channel, velocity curve, range and fixed velocity, key and velocity range (splits), and CC, pitch bend, aftertouch and program change filters.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'u', 't'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { reset(); }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
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
        taps.inLevel.store(float(fDsp.lastOut() + 1), std::memory_order_relaxed);
        taps.outLevel.store(float(fDsp.lastVelocity()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.lastVelocityOut()), std::memory_order_relaxed);
    }

private:
    // @init then @slider
    void reset()
    {
        fDsp.reset();
        update();
    }

    int iv(uint32_t i) const { return int(std::lround(value(i))); }

    void update()
    {
        nullmidiutil::Params p;
        p.transpose = iv(0);
        p.channel = iv(1);
        p.curve = value(2);
        p.velMin = iv(3);
        p.velMax = iv(4);
        p.fixed = iv(5);
        p.keyLow = iv(6);
        p.keyHigh = iv(7);
        p.velLow = iv(8);
        p.velHigh = iv(9);
        p.blockCC = iv(10) != 0;
        p.blockBend = iv(11) != 0;
        p.blockTouch = iv(12) != 0;
        p.blockProgram = iv(13) != 0;
        fDsp.setParams(p);
    }

    nullmidiutil::MidiUtilityProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMidiUtility)
};

Plugin* createPlugin()
{
    return new PluginMidiUtility();
}

END_NAMESPACE_DISTRHO
