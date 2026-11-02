/*
 * NULL JSFX - Modal Piano (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. For the display: aux[0..7] = the keys sounding (a bit per
 * key, 16 per word), aux[8] = the pedals (1 sustain, 2 soft), extra = the voices in use.
 */

#include "NullPlugin.hpp"
#include "ModalPianoDSP.hpp"
#include "ModalPianoSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginModalPiano : public NullMultiPlugin
{
public:
    PluginModalPiano()
        : NullMultiPlugin(nullpiano::kSliders, nullpiano::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "ModalPiano"; }
    const char* getDescription() const override
    {
        return "A piano from physics, no samples: modal synthesis of stiff strings with double decay and unison beats, hammer, dampers, sustain and soft pedals, body and stereo keys.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'P', 'n'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setParams(fLast);
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

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullpiano::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > 3)
                continue;
            fEvents[n++] = { ev.frame, double(ev.data[0]), ev.size > 1 ? double(ev.data[1]) : 0.0,
                             ev.size > 2 ? double(ev.data[2]) : 0.0 };
        }
        fDsp.beginBlock(fEvents, n);
        fDsp.process(outputs[0], outputs[1], frames);

        uint32_t bits[8];
        fDsp.soundingBits(bits);
        for (int i = 0; i < 8; ++i)
            taps.aux[i].store(float(bits[i]), std::memory_order_relaxed);
        taps.aux[8].store(float(fDsp.pedals()), std::memory_order_relaxed);
        taps.extra.store(float(fDsp.voices()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullpiano::kNumParams; ++i)
            fLast[i] = jsfxValue(nullpiano::kSliders[i], value(i));
        fDsp.setParams(fLast);
    }

    nullpiano::ModalPiano fDsp;
    nullpiano::MidiEv fEvents[nullpiano::kMaxEvents];
    double fLast[nullpiano::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginModalPiano)
};

Plugin* createPlugin()
{
    return new PluginModalPiano();
}

END_NAMESPACE_DISTRHO
