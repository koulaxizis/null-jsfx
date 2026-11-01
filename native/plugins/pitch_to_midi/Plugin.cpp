/*
 * NULL JSFX - Pitch to MIDI (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect that listens to the audio: the audio passes through, the MIDI that comes in
 * passes through at its frame (SysEx, which the JSFX never sees, too), and a monophonic line
 * becomes notes (PitchToMidiProcessor, pitch_to_midi.jsfx's @block and @sample). For the
 * display: inLevel = the analysis level in dB, gr = the pitch found (-1 = none), extra = the note
 * sent (-1 = none).
 */

#include "NullPlugin.hpp"
#include "NullMidiNotes.hpp"
#include "PitchToMidiDSP.hpp"
#include "PitchToMidiSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPitchToMidi : public NullMultiPlugin
{
public:
    PluginPitchToMidi()
        : NullMultiPlugin(nullpitchtomidi::kSliders, nullpitchtomidi::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "PitchToMidi"; }
    const char* getDescription() const override
    {
        return "Turns a monophonic line (bass, voice, a lead) into MIDI notes: threshold, note range, stability, velocity, transpose, pitch bend and channel.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', '2', 'M'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        fDsp.begin(frames);
        for (uint32_t i = 0; i < midiCount; ++i)
            if (midi[i].size >= 1 && midi[i].size <= 3)
                fDsp.input(midi[i].frame, midi[i].data[0], midi[i].size > 1 ? midi[i].data[1] : 0,
                           midi[i].size > 2 ? midi[i].data[2] : 0);
        uint32_t sx = 0;
        auto sysexUpTo = [&](uint32_t frame) {
            for (; sx < midiCount; ++sx)
            {
                if (midi[sx].size <= 3)
                    continue;
                if (midi[sx].frame > frame)
                    break;
                writeMidiEvent(midi[sx]);
            }
        };
        auto send = [&](uint32_t ofs, int a, int b, int c) {
            sysexUpTo(ofs);
            MidiEvent ev {};
            ev.frame = ofs;
            ev.size = nulljsfx::MidiNotes::size(a);
            ev.data[0] = uint8_t(a);
            ev.data[1] = uint8_t(b);
            ev.data[2] = uint8_t(c);
            ev.dataExt = nullptr;
            writeMidiEvent(ev);
        };
        for (uint32_t i = 0; i < frames; ++i)
            fDsp.processSample(inputs[0][i], inputs[1][i], send);
        sysexUpTo(frames);
        for (int c = 0; c < 2; ++c)
            if (outputs[c] != inputs[c])
                std::memmove(outputs[c], inputs[c], sizeof(float) * frames);
        taps.inLevel.store(float(fDsp.detectedLevel()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.detectedPitch()), std::memory_order_relaxed);
        taps.extra.store(float(fDsp.sentNote()), std::memory_order_relaxed);
    }

private:
    void setup(double sr) { fDsp.setSampleRate(sr); }

    void update()
    {
        fDsp.setParams(nullpitchtomidi::paramsFrom([this](uint32_t i) { return nullpitchtomidi::sliderValue(i, value(i)); }));
    }

    nullpitchtomidi::PitchToMidiProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPitchToMidi)
};

Plugin* createPlugin()
{
    return new PluginPitchToMidi();
}

END_NAMESPACE_DISTRHO
