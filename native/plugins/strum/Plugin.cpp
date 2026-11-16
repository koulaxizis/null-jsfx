/*
 * NULL JSFX - Strum (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio and the other MIDI pass through (SysEx, which the JSFX never sees,
 * too), and the held chord is strummed (StrumProcessor, strum.jsfx's @block and @sample).
 * For the display: extra = the strums, gr = the rhythm's step (-1 when none runs), aux[0] =
 * the keys held.
 */

#include "NullPlugin.hpp"
#include "NullMidiNotes.hpp"
#include "StrumDSP.hpp"
#include "StrumSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginStrum : public NullMultiPlugin
{
public:
    PluginStrum()
        : NullMultiPlugin(nullstrum::kSliders, nullstrum::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Strum"; }
    const char* getDescription() const override
    {
        return "Plays the held chord like a guitar pick: down and up strokes, speed, rhythm patterns on the tempo, velocity per string and humanize.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 't', 'm'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        fDsp.begin(frames, tempo);
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
            fDsp.processSample(send);
        sysexUpTo(frames);
        for (int c = 0; c < 2; ++c)
            if (outputs[c] != inputs[c])
                std::memmove(outputs[c], inputs[c], sizeof(float) * frames);
        taps.extra.store(float(fDsp.strums()), std::memory_order_relaxed);
        taps.gr.store(fDsp.running() ? float(fDsp.step()) : -1.0f, std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.held()), std::memory_order_relaxed);
    }

private:
    void setup(double sr) { fDsp.setSampleRate(sr); }

    void update()
    {
        fDsp.setParams(nullstrum::paramsFrom([this](uint32_t i) { return nullstrum::sliderValue(i, value(i)); }));
    }

    nullstrum::StrumProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStrum)
};

Plugin* createPlugin()
{
    return new PluginStrum();
}

END_NAMESPACE_DISTRHO
