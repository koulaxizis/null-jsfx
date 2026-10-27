/*
 * NULL JSFX - Scale (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio passes through, the MIDI goes through ScaleProcessor (scale.jsfx's
 * @block). SysEx, which the JSFX never sees, passes through at its frame. For the display:
 * aux[0..7] = the output notes sounding (16 notes per value, as bits), aux[8..15] = the keys
 * held, extra = 1 + the last key played (0 = none yet), inLevel = 1 + the note it played
 * (0 = none: dropped, or nothing yet).
 */

#include "NullPlugin.hpp"
#include "ScaleDSP.hpp"
#include "ScaleSliders.hpp"
#include "NullMidiTaps.hpp"

START_NAMESPACE_DISTRHO

class PluginScale : public NullMultiPlugin
{
public:
    PluginScale()
        : NullMultiPlugin(nullscale::kSliders, nullscale::kNumSliders)
    {
        reset();
    }

protected:
    const char* getLabel() const override { return "Scale"; }
    const char* getDescription() const override
    {
        return "MIDI effect: every note moves into the chosen key and scale (nearest, up or down), or out-of-scale notes are dropped, or the white keys play the scale's degrees; with transpose and octave.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'c', 'l'); }

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
    }

private:
    // @init then @slider
    void reset()
    {
        fDsp.reset();
        update();
    }

    void update()
    {
        nullscale::Params p;
        p.key = int(std::lround(value(0)));
        p.scale = int(std::lround(value(1)));
        p.mode = int(std::lround(value(2)));
        p.transpose = int(std::lround(value(3)));
        p.octave = int(std::lround(value(4)));
        fDsp.setParams(p);
    }

    nullscale::ScaleProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScale)
};

Plugin* createPlugin()
{
    return new PluginScale();
}

END_NAMESPACE_DISTRHO
