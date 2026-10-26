/*
 * NULL JSFX - Organ (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. For the display: aux[0], aux[1] = the horn's and the
 * drum's angle (0..1), aux[2], aux[3] = their speeds (Hz), aux[4] = the expression, aux[5] =
 * the rotor setting in effect (with the mod wheel), extra = the voices sounding.
 */

#include "NullPlugin.hpp"
#include "OrganDSP.hpp"
#include "OrganSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginOrgan : public NullMultiPlugin
{
public:
    PluginOrgan()
        : NullMultiPlugin(nullorgan::kSliders, nullorgan::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Organ"; }
    const char* getDescription() const override
    {
        return "Tonewheel organ with nine drawbars, percussion, key click, vibrato and chorus, drive and a rotary speaker; CC 11 is expression, the mod wheel switches the rotor speed.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'O', 'r', 'g'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setParams(fLast);
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        // the JSFX sends every message it receives on (midisend in @block); SysEx, which the
        // JSFX never sees, passes through there as well
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]);

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullorgan::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > 3)
                continue;
            fEvents[n++] = { ev.frame, double(ev.data[0]), ev.size > 1 ? double(ev.data[1]) : 0.0,
                             ev.size > 2 ? double(ev.data[2]) : 0.0 };
        }
        fDsp.beginBlock(fEvents, n);
        fDsp.process(outputs[0], outputs[1], frames);

        taps.aux[0].store(float(fDsp.hornPh), std::memory_order_relaxed);
        taps.aux[1].store(float(fDsp.drumPh), std::memory_order_relaxed);
        taps.aux[2].store(float(fDsp.hornF), std::memory_order_relaxed);
        taps.aux[3].store(float(fDsp.drumF), std::memory_order_relaxed);
        taps.aux[4].store(float(fDsp.ex), std::memory_order_relaxed);
        taps.aux[5].store(float(fDsp.rotEff), std::memory_order_relaxed);
        taps.extra.store(float(fDsp.voices()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullorgan::kNumParams; ++i)
        {
            const SliderInfo& in = nullorgan::kSliders[i];
            fLast[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                      : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                fLast[i] = std::round(fLast[i] * 10.0) / 10.0;
        }
        fDsp.setParams(fLast);
    }

    nullorgan::Organ fDsp;
    nullorgan::MidiEv fEvents[nullorgan::kMaxEvents];
    double fLast[nullorgan::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginOrgan)
};

Plugin* createPlugin()
{
    return new PluginOrgan();
}

END_NAMESPACE_DISTRHO
