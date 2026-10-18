/*
 * NULL JSFX - Glitch FX (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * MIDI notes C3..F3 drive Trigger MIDI at their frames, and every MIDI message passes through
 * as the JSFX's midisend does. For the display: extra = the tempo, aux[0] = the held effects
 * (bit k = note 60 + k).
 */

#include "NullPlugin.hpp"
#include "GlitchFxDSP.hpp"
#include "GlitchFxSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginGlitchFx : public NullMultiPlugin
{
public:
    PluginGlitchFx()
        : NullMultiPlugin(nullglitch::kSliders, nullglitch::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "GlitchFX"; }
    const char* getDescription() const override
    {
        return "Tempo-synced stutter with gate, pitch shift, random reverse, tape stop and granular variations, rolled at random or played from MIDI notes.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '7', 'G', 'l'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount; ++i)
        {
            const MidiEvent& ev = midi[i];
            writeMidiEvent(ev); // every message passes through
            const uint8_t* d = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
            if (ev.size >= 3 && n < kMaxMidi)
                fMidi[n++] = { ev.frame, d[0], d[1], d[2] };
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo, fMidi, n);
        taps.extra.store(float(tempo), std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.heldMask()), std::memory_order_relaxed);
    }

private:
    static constexpr uint32_t kMaxMidi = 256;

    void update()
    {
        fDsp.setParams(nullglitch::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullglitch::GlitchProcessor fDsp;
    nullglitch::MidiMsg fMidi[kMaxMidi];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGlitchFx)
};

Plugin* createPlugin()
{
    return new PluginGlitchFx();
}

END_NAMESPACE_DISTRHO
