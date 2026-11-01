/*
 * NULL JSFX - Drum Trigger (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect that listens to the audio: the audio passes through, the MIDI that comes in
 * passes through at its frame (SysEx, which the JSFX never sees, too), and every hit adds a
 * note (DrumTriggerProcessor, drum_trigger.jsfx's @block and @sample). For the display:
 * inLevel = the detector's peak (held 0.3 s), extra = the hits so far, gr = the last velocity,
 * aux[0] = 1 while the note sounds.
 */

#include "NullPlugin.hpp"
#include "NullMidiNotes.hpp"
#include "DrumTriggerDSP.hpp"
#include "DrumTriggerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDrumTrigger : public NullMultiPlugin
{
public:
    PluginDrumTrigger()
        : NullMultiPlugin(nulldrumtrigger::kSliders, nulldrumtrigger::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "DrumTrigger"; }
    const char* getDescription() const override
    {
        return "Turns the hits of a recorded drum into MIDI notes with velocity, for replacing or layering it with "
               "an instrument: note, threshold, retrigger, dynamics, detector filters, note length and channel.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'T', 'r'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        fDsp.begin();
        for (uint32_t i = 0; i < midiCount; ++i)
            if (midi[i].size >= 1 && midi[i].size <= 3)
                fDsp.input(midi[i].frame, frames, midi[i].data[0], midi[i].size > 1 ? midi[i].data[1] : 0,
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
        {
            const float l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r, send);
            fIn.process(float(fDsp.envelope()));
        }
        sysexUpTo(frames);
        for (int c = 0; c < 2; ++c)
            if (outputs[c] != inputs[c])
                std::memmove(outputs[c], inputs[c], sizeof(float) * frames);
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.extra.store(float(fDsp.hits()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.lastVelocity()), std::memory_order_relaxed);
        taps.aux[0].store(fDsp.sounding() ? 1.0f : 0.0f, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
    }

    void update()
    {
        fDsp.setParams(nulldrumtrigger::paramsFrom([this](uint32_t i) { return nulldrumtrigger::sliderValue(i, value(i)); }));
    }

    nulldrumtrigger::DrumTriggerProcessor fDsp;
    nulljsfx::PeakFollower fIn;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDrumTrigger)
};

Plugin* createPlugin()
{
    return new PluginDrumTrigger();
}

END_NAMESPACE_DISTRHO
