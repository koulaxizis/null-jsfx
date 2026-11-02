/*
 * NULL JSFX - String Ensemble (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. For the display: aux[0..7] = the keys sounding (a bit per
 * key, 16 per word), extra = how many.
 */

#include "NullPlugin.hpp"
#include "StringEnsembleDSP.hpp"
#include "StringEnsembleSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginStringEnsemble : public NullMultiPlugin
{
public:
    PluginStringEnsemble()
        : NullMultiPlugin(nullstrings::kSliders, nullstrings::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "StringEnsemble"; }
    const char* getDescription() const override
    {
        return "A 1970s string machine from synthesis: a voice per key, 16', 8' and 4' sawtooths, attack and release, brightness, body and a three-tap ensemble chorus.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 't', 'E'); }

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
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullstrings::kMaxEvents); ++i)
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
        taps.extra.store(float(fDsp.keys()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullstrings::kNumParams; ++i)
            fLast[i] = jsfxValue(nullstrings::kSliders[i], value(i));
        fDsp.setParams(fLast);
    }

    nullstrings::StringEnsemble fDsp;
    nullstrings::MidiEv fEvents[nullstrings::kMaxEvents];
    double fLast[nullstrings::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStringEnsemble)
};

Plugin* createPlugin()
{
    return new PluginStringEnsemble();
}

END_NAMESPACE_DISTRHO
