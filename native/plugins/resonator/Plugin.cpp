/*
 * NULL JSFX - Resonator (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * MIDI notes reach the DSP at their frames (Notes MIDI), and every MIDI message passes through
 * as the JSFX's midisend does.
 */

#include "NullPlugin.hpp"
#include "ResonatorDSP.hpp"
#include "ResonatorSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginResonator : public NullMultiPlugin
{
public:
    PluginResonator()
        : NullMultiPlugin(nullres::kSliders, nullres::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Resonator"; }
    const char* getDescription() const override
    {
        return "Tuned string resonators: makes any input ring at a root note and chord or at held MIDI notes, with decay, brightness and stereo spread.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 'e', 's'); }

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

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]); // every message passes through
        uint32_t mi = 0;
        for (uint32_t i = 0; i < frames; ++i)
        {
            for (; mi < midiCount && midi[mi].frame <= i; ++mi)
            {
                const uint8_t* d = midi[mi].size > MidiEvent::kDataSize ? midi[mi].dataExt : midi[mi].data;
                if (midi[mi].size >= 3)
                    fDsp.midi(d[0], d[1], d[2]);
            }
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
        }
        // for the display: Notes MIDI's tuning (aux[0] = how many, aux[1..4] = the notes)
        taps.aux[0].store(float(fDsp.tunedCount()), std::memory_order_relaxed);
        for (int k = 0; k < 4; ++k)
            taps.aux[1 + k].store(float(fDsp.tunedNote(k)), std::memory_order_relaxed);
    }

private:
    void update()
    {
        double s[nullres::kNumSliders];
        for (uint32_t i = 0; i < nullres::kNumSliders; ++i)
            s[i] = nullres::sliderValue(i, value(i));
        fDsp.setParams(s);
    }

    nullres::ResonatorProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginResonator)
};

Plugin* createPlugin()
{
    return new PluginResonator();
}

END_NAMESPACE_DISTRHO
