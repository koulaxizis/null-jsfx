/*
 * NULL JSFX - Keys (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. For the display: aux[0..7] = the notes sounding (a bit
 * per note, 16 per word), extra = the voices sounding.
 */

#include "NullPlugin.hpp"
#include "KeysDSP.hpp"
#include "KeysSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginKeys : public NullMultiPlugin
{
public:
    PluginKeys()
        : NullMultiPlugin(nullkeys::kSliders, nullkeys::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Keys"; }
    const char* getDescription() const override
    {
        return "Electric pianos, music box and mallets from FM and modal synthesis (no samples), 16 voices, with tremolo or autopan, drive and chorus; sustain pedal.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'K', 'e', 'y'); }

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
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullkeys::kMaxEvents); ++i)
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
        taps.extra.store(float(fDsp.voices()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullkeys::kNumParams; ++i)
        {
            const SliderInfo& in = nullkeys::kSliders[i];
            // back on the JSFX's grid in double (a float 0.1 step is not 0.1)
            fLast[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                      : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                fLast[i] = std::round(fLast[i] * 10.0) / 10.0;
        }
        fDsp.setParams(fLast);
    }

    nullkeys::Keys fDsp;
    nullkeys::MidiEv fEvents[nullkeys::kMaxEvents];
    double fLast[nullkeys::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginKeys)
};

Plugin* createPlugin()
{
    return new PluginKeys();
}

END_NAMESPACE_DISTRHO
