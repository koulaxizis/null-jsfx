/*
 * NULL JSFX - Music Box (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. For the display: aux[0..7] = the tines ringing (a bit
 * per note, 16 per word), extra = the tines ringing, gr = the spring (1 wound, 0 run down, -1
 * stopped).
 */

#include "NullPlugin.hpp"
#include "MusicBoxDSP.hpp"
#include "MusicBoxSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginMusicBox : public NullMultiPlugin
{
public:
    PluginMusicBox()
        : NullMultiPlugin(nullmusicbox::kSliders, nullmusicbox::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Music Box"; }
    const char* getDescription() const override
    {
        return "Music box from modal synthesis (no samples), 32 tines: a steel comb plucked by pins, a wooden box, the pin click and governor whirr, and a wind-down as the spring runs out.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'B', 'x'); }

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
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullmusicbox::kMaxEvents); ++i)
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
        taps.extra.store(float(fDsp.tines()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.spring()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullmusicbox::kNumParams; ++i)
        {
            const SliderInfo& in = nullmusicbox::kSliders[i];
            // back on the JSFX's grid in double (a float 0.1 step is not 0.1)
            fLast[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                      : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                fLast[i] = std::round(fLast[i] * 10.0) / 10.0;
        }
        fDsp.setParams(fLast);
    }

    nullmusicbox::MusicBox fDsp;
    nullmusicbox::MidiEv fEvents[nullmusicbox::kMaxEvents];
    double fLast[nullmusicbox::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMusicBox)
};

Plugin* createPlugin()
{
    return new PluginMusicBox();
}

END_NAMESPACE_DISTRHO
