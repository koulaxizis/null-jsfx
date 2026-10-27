/*
 * NULL JSFX - Volume Shaper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Inputs 3/4 are the sidechain key (JSFX pins 3/4) for Trigger Sidechain; MIDI note-ons drive
 * Trigger MIDI, and every MIDI message passes through as the JSFX's midisend does.
 */

#include "NullPlugin.hpp"
#include "VolumeShaperDSP.hpp"
#include "VolumeShaperSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginVolumeShaper : public NullMultiPlugin
{
public:
    PluginVolumeShaper()
        : NullMultiPlugin(nullvolshaper::kSliders, nullvolshaper::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        update();
    }

protected:
    const char* getLabel() const override { return "VolumeShaper"; }
    const char* getDescription() const override
    {
        return "Tempo-synced volume envelope: pump, saw, gate, trance, sine and stutter shapes with rate, depth, "
               "smoothing, phase and mix, looped on the tempo or played once per MIDI note or sidechain hit.";
    }

    // inputs 1/2: the track (stereo), inputs 3/4: the sidechain key (JSFX pins 3/4)
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input && index >= 2)
        {
            port.hints = kAudioPortIsSidechain;
            port.name = index == 2 ? "Sidechain Left" : "Sidechain Right";
            port.symbol = index == 2 ? "sidechain_left" : "sidechain_right";
            return;
        }
        port.groupId = kPortGroupStereo;
        NullMultiPlugin::initAudioPort(input, index, port);
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'V', 'S', 'h'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
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
        // MIDI: every message passes through; the note-ons' frames go to Trigger MIDI
        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount; ++i)
        {
            const MidiEvent& ev = midi[i];
            writeMidiEvent(ev);
            const uint8_t* d = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
            if (ev.size >= 3 && (d[0] & 0xF0) == 0x90 && d[2] > 0 && n < kMaxMidi)
                fNoteOns[n++] = ev.frame;
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tp.playing, beatPos, tempo, inputs[2],
                     inputs[3], fNoteOns, n);
        // the cycle position (playhead) and the smoothed gain (before Mix), for the display
        taps.extra.store(float(fDsp.position()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.gain()), std::memory_order_relaxed);
    }

private:
    static constexpr uint32_t kMaxMidi = 256;

    void update()
    {
        fDsp.setParams(nullvolshaper::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullvolshaper::VolumeShaperProcessor fDsp;
    uint32_t fNoteOns[kMaxMidi] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginVolumeShaper)
};

Plugin* createPlugin()
{
    return new PluginVolumeShaper();
}

END_NAMESPACE_DISTRHO
