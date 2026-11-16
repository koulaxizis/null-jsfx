/*
 * NULL JSFX - Synth (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. MIDI (notes, sustain, mod wheel, pitch bend, all notes
 * off) is played at its frame like the JSFX's @block queue; the host tempo and transport drive
 * the synced LFO and the Delay. The interface gets each voice's amp envelope (aux[0..7]) and
 * the LFO (extra).
 */

#include "NullPlugin.hpp"
#include "SynthDSP.hpp"
#include "SynthSliders.hpp"
#include "NullMailbox.hpp"
#include "../sampler/SamplerRead.hpp"

#include <cstring>
#include <vector>

START_NAMESPACE_DISTRHO

class PluginSynth : public NullMultiPlugin
{
public:
    PluginSynth()
        : NullMultiPlugin(nullsynth::kSliders, nullsynth::kNumParams, 1)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Synth"; }
    const char* getDescription() const override
    {
        return "Polyphonic synthesizer with Analog and Wavetable engines, filter, envelopes, LFO, unison, Character and built-in chorus, delay and reverb.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'y', 'n'); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input)
            return NullMultiPlugin::initAudioPort(input, index, port);
        port.groupId = kPortGroupStereo;
        port.name = index == 0 ? "Left" : "Right";
        port.symbol = index == 0 ? "out_left" : "out_right";
    }

    void valueChanged(uint32_t, float) override { update(); }

    // 4.0: the Wavetable File (slider48 of the JSFX) is the "wavetable" state, a file path the
    // host saves with the project
    void initState(uint32_t index, State& state) override
    {
        if (index != 0)
            return;
        state.hints = kStateIsFilenamePath;
        state.key = "wavetable";
        state.defaultValue = "";
        state.label = "Wavetable File";
        state.description = "WAV or FLAC wavetable the Wavetable engine plays instead of Table (none: the built-in tables)";
    }

    // the file read and turned into the user table off the audio thread (wt_user_load), handed
    // to process() without a lock
    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "wavetable") != 0)
            return;
        Pending* p = new Pending();
        nullsmp::SampleFile file;
        if (nullsmp::readSample(value, file, nullsynth::userFileMax(getSampleRate())))
            p->ok = nullsynth::userTable(file.data.data(), file.data.size(), file.channels, p->table);
        fFiles.post(p);
    }

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

    void process(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        // the JSFX sends every message it receives on (midisend in @block); SysEx, which the
        // JSFX never sees, passes through there as well
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]);

        if (Pending* p = fFiles.take())
        {
            fUser.swap(p->table); // the old table goes back in p, freed off the audio thread
            fDsp.setUserTable(p->ok ? fUser.data() : nullptr);
            fFiles.giveBack(p);
        }

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

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullsynth::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > MidiEvent::kDataSize)
                continue;
            const uint8_t* d = ev.data;
            if (d[0] < 0x80 || d[0] >= 0xF0)
                continue;
            fEvents[n++] = { ev.frame, d[0], uint8_t(ev.size > 1 ? d[1] : 0), uint8_t(ev.size > 2 ? d[2] : 0) };
        }
        fDsp.beginBlock(fEvents, n, tp.playing, tempo, beatPos);
        fDsp.process(outputs[0], outputs[1], frames);

        for (int v = 0; v < nullsynth::kVoices; ++v)
        {
            const nullsynth::Synth::Voice& vo = fDsp.voice(v);
            taps.aux[v].store(vo.st > 0 ? float(vo.ae) : 0.0f, std::memory_order_relaxed);
        }
        taps.extra.store(float(fDsp.lfoValue()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        double v[nullsynth::kNumSliders];
        for (uint32_t i = 0; i < nullsynth::kNumSliders; ++i)
        {
            const SliderInfo& in = nullsynth::kSliders[i];
            // back on the JSFX's grid in double (a float 0.01 step is not 0.01)
            double x = double(value(i));
            if (in.step > 0.0f)
            {
                x = double(in.min) + std::round((x - in.min) / double(in.step)) * double(in.step);
                if (in.step < 0.05f)
                    x = std::round(x * 100.0) / 100.0;
                else if (in.step < 1.0f)
                    x = std::round(x * 10.0) / 10.0;
            }
            v[i] = x;
        }
        fDsp.setParams(v);
    }

    nullsynth::Synth fDsp;
    std::vector<double> fUser; // the user table the DSP reads
    struct Pending {
        std::vector<double> table;
        bool ok = false;
    };
    nulljsfx::Mailbox<Pending> fFiles;
    nullsynth::MidiEv fEvents[nullsynth::kMaxEvents];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSynth)
};

Plugin* createPlugin()
{
    return new PluginSynth();
}

END_NAMESPACE_DISTRHO
