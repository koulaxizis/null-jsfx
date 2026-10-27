/*
 * NULL JSFX - Riff (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio passes through, the MIDI goes through RiffProcessor (riff.jsfx's
 * @block). SysEx, which the JSFX never sees, passes through at its frame. For the display:
 * extra = 1 + the step playing (0 = no riff), aux[0] = 1 + the leading key, aux[1] = 1 + the
 * note sounding (0 = none).
 */

#include "NullPlugin.hpp"
#include "RiffDSP.hpp"
#include "RiffSliders.hpp"
#include "NullMidiTaps.hpp"

START_NAMESPACE_DISTRHO

class PluginRiff : public NullMultiPlugin
{
public:
    PluginRiff()
        : NullMultiPlugin(nullriff::kSliders, nullriff::kNumSliders)
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Riff"; }
    const char* getDescription() const override
    {
        return "MIDI effect: holding a key plays a tempo-synced riff generated from a seed, in the chosen key and scale and starting from the key held, with density, steps, rate, octave range, note length, accents, velocity range, variation and a rhythm lock.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 'i', 'f'); }

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
        double beatPos = 0.0;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beatPos = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) +
                          tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beatPos = double(tp.frame) / getSampleRate() * tempo / 60.0;
        }
        fDsp.begin(tempo, tp.playing, beatPos);
        for (uint32_t i = 0; i < midiCount; ++i)
            if (midi[i].size >= 1 && midi[i].size <= 3)
                fDsp.input(midi[i].frame, midi[i].data[0], midi[i].size > 1 ? midi[i].data[1] : 0,
                           midi[i].size > 2 ? midi[i].data[2] : 0);
        nullmidi::sendMidi(*this, fDsp, frames, midi, midiCount);
        taps.extra.store(float(fDsp.step() + 1), std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.root() + 1), std::memory_order_relaxed);
        taps.aux[1].store(float(fDsp.sounding() + 1), std::memory_order_relaxed);
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
        nullriff::Params p;
        p.key = iv(0);
        p.scale = iv(1);
        p.seed = iv(2);
        p.dens = double(iv(3)) / 100.0;
        p.steps = iv(4);
        p.rate = iv(5);
        p.octs = iv(6);
        p.nlen = double(iv(7)) / 100.0;
        p.lock = iv(8);
        p.vrng = double(iv(9)) / 100.0;
        p.acc = double(iv(10)) / 100.0;
        p.vari = double(iv(11)) / 100.0;
        fDsp.setParams(p);
    }

    nullriff::RiffProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRiff)
};

Plugin* createPlugin()
{
    return new PluginRiff();
}

END_NAMESPACE_DISTRHO
