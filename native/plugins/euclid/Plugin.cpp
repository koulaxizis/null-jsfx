/*
 * NULL JSFX - Euclid (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect: the audio and the MIDI that comes in pass through (SysEx, which the JSFX never
 * sees, too), and a Euclidean pattern adds notes on the host's beat (EuclidProcessor, euclid.jsfx's
 * @block and @sample). For the display: extra = the step that plays + 1 (0 = none), aux[0] = 1
 * while the note sounds, inLevel = 1 while the host plays.
 */

#include "NullPlugin.hpp"
#include "NullMidiNotes.hpp"
#include "EuclidDSP.hpp"
#include "EuclidSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginEuclid : public NullMultiPlugin
{
public:
    PluginEuclid()
        : NullMultiPlugin(nulleuclid::kSliders, nulleuclid::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Euclid"; }
    const char* getDescription() const override
    {
        return "A Euclidean rhythm generator: pulses spread evenly over steps, with rotate, rate, note, velocity, accent, gate, swing, run mode and channel.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'E', 'u', 'c'); }

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
        bool playing = tp.playing;
        double beat = 0.0;
        if (playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beat = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) + tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beat = double(tp.frame) / getSampleRate() * tempo / 60.0;
        }
        fDsp.begin(frames, playing, tempo, beat);
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
        taps.extra.store(float(fDsp.currentStep() + 1), std::memory_order_relaxed);
        taps.aux[0].store(fDsp.sounding() ? 1.0f : 0.0f, std::memory_order_relaxed);
        taps.inLevel.store(playing ? 1.0f : 0.0f, std::memory_order_relaxed);
    }

private:
    void setup(double sr) { fDsp.setSampleRate(sr); }

    void update()
    {
        fDsp.setParams(nulleuclid::paramsFrom([this](uint32_t i) { return nulleuclid::sliderValue(i, value(i)); }));
    }

    nulleuclid::EuclidProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEuclid)
};

Plugin* createPlugin()
{
    return new PluginEuclid();
}

END_NAMESPACE_DISTRHO
