/*
 * NULL JSFX - Wind (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. For the display: aux[0] = the note sounding (-1 for
 * none), aux[1] = the breath (0..1).
 */

#include "NullPlugin.hpp"
#include "WindDSP.hpp"
#include "WindSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginWind : public NullMultiPlugin
{
public:
    PluginWind()
        : NullMultiPlugin(nullwind::kSliders, nullwind::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Wind"; }
    const char* getDescription() const override
    {
        return "Flute, clarinet, saxophone and brass by physical modelling (digital waveguides), monophonic with glide, the breath from velocity, the mod wheel, breath CC, aftertouch or expression.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'W', 'n', 'd'); }

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
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullwind::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > 3)
                continue;
            fEvents[n++] = { ev.frame, double(ev.data[0]), ev.size > 1 ? double(ev.data[1]) : 0.0,
                             ev.size > 2 ? double(ev.data[2]) : 0.0 };
        }
        fDsp.beginBlock(fEvents, n);
        fDsp.process(outputs[0], outputs[1], frames);

        taps.aux[0].store(float(fDsp.noteNow()), std::memory_order_relaxed);
        taps.aux[1].store(float(fDsp.breathNow()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullwind::kNumParams; ++i)
            fLast[i] = jsfxValue(nullwind::kSliders[i], value(i));
        fDsp.setParams(fLast);
    }

    nullwind::Wind fDsp;
    nullwind::MidiEv fEvents[nullwind::kMaxEvents];
    double fLast[nullwind::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWind)
};

Plugin* createPlugin()
{
    return new PluginWind();
}

END_NAMESPACE_DISTRHO
