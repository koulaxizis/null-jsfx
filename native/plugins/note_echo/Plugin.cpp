/*
 * NULL JSFX - Note Echo (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio and the other MIDI pass through (SysEx, which the JSFX never sees,
 * too), and every note that comes in is played again (NoteEchoProcessor, note_echo.jsfx's
 * @block and @sample). For the display: extra = the echoes sent, gr = the interval in ms.
 */

#include "NullPlugin.hpp"
#include "NullMidiNotes.hpp"
#include "NoteEchoDSP.hpp"
#include "NoteEchoSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginNoteEcho : public NullMultiPlugin
{
public:
    PluginNoteEcho()
        : NullMultiPlugin(nullnoteecho::kSliders, nullnoteecho::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "NoteEcho"; }
    const char* getDescription() const override
    {
        return "A delay for notes: repeats at a time or a synced note value, with velocity decay, pitch step, gate and dry on or off.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'N', 'E', 'c'); }

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
        taps.extra.store(float(fDsp.echoes()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.interval() / getSampleRate() * 1000.0), std::memory_order_relaxed);
    }

private:
    void setup(double sr) { fDsp.setSampleRate(sr); }

    void update()
    {
        fDsp.setParams(nullnoteecho::paramsFrom([this](uint32_t i) { return nullnoteecho::sliderValue(i, value(i)); }));
    }

    nullnoteecho::NoteEchoProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginNoteEcho)
};

Plugin* createPlugin()
{
    return new PluginNoteEcho();
}

END_NAMESPACE_DISTRHO
